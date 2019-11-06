import os
import vtk, qt, ctk, slicer
import logging
from SegmentEditorEffects import *

class SegmentEditorThreshold2Effect(SegmentEditorThresholdEffect):
  """ ThresholdEffect is an Effect implementing the TODO
  """

  def __init__(self, scriptedEffect):
    SegmentEditorThresholdEffect.__init__(self, scriptedEffect)
    scriptedEffect.name = 'Threshold 2'

    backgroundValue = 0
    labelValue = 1

    # Erode + Dilate + Island pipeline
    self.thresh = vtk.vtkImageThreshold()
    self.thresh.SetInValue(labelValue)
    self.thresh.SetOutValue(backgroundValue)

    self.floodFillingFilter = vtk.vtkImageThresholdConnectivity()
    self.dilate = vtk.vtkImageDilateErode3D()

    self.erode = vtk.vtkImageDilateErode3D()
    self.erode.SetInputConnection(self.thresh.GetOutputPort())
    self.erode.SetDilateValue(backgroundValue)
    self.erode.SetErodeValue(labelValue)

    self.floodFillingFilter.SetInValue(labelValue)
    self.floodFillingFilter.SetOutValue(backgroundValue)
    self.floodFillingFilter.ThresholdBetween(labelValue, labelValue)
    self.floodFillingFilter.SetInputConnection(self.erode.GetOutputPort())

    self.dilate.SetInputConnection(self.floodFillingFilter.GetOutputPort())
    self.dilate.SetDilateValue(labelValue)
    self.dilate.SetErodeValue(backgroundValue)


  def clone(self):
    import qSlicerSegmentationsEditorEffectsPythonQt as effects
    clonedEffect = effects.qSlicerSegmentEditorScriptedEffect(None)
    clonedEffect.setPythonSource(__file__.replace('\\','/'))
    return clonedEffect

  def icon(self):
    iconPath = os.path.join(os.path.dirname(__file__), 'Resources/Icons/Threshold.png')
    if os.path.exists(iconPath):
      return qt.QIcon(iconPath)
    return qt.QIcon()

  def helpText(self):
    return """<html>Fill segment based on master volume intensity range<br>. Options:<p>
<ul style="margin: 0">
<li><b>Use for masking:</b> set the selected intensity range as <dfn>Editable intensity range</dfn> and switch to Paint effect.</li>
<li><b>Apply:</b> set the previewed segmentation in the selected segment. Previous contents of the segment is overwritten.</li>
</ul><p></html>"""

  def setCurrentSegmentTransparent(self):
    pass

  def restorePreviewedSegmentTransparency(self):
    pass

  def setupOptionsFrame(self):
    SegmentEditorThresholdEffect.setupOptionsFrame(self)

    # Hide threshold options
    self.applyButton.setHidden(True)
    self.useForPaintButton.setHidden(True)

    # Add margin options

    self.marginSizeMmSpinBox = slicer.qMRMLSpinBox()
    self.marginSizeMmSpinBox.setMRMLScene(slicer.mrmlScene)
    self.marginSizeMmSpinBox.quantity = "length"
    self.marginSizeMmSpinBox.value = 3.0
    self.marginSizeMmSpinBox.singleStep = 1.0
    self.kernelSizePixel = qt.QLabel()
    self.kernelSizePixel.setToolTip("Size change in pixels. Computed from the segment's spacing and the specified margin size.")
    marginSizeFrame = qt.QHBoxLayout()
    marginSizeFrame.addWidget(self.marginSizeMmSpinBox)
    marginSizeFrame.addWidget(self.kernelSizePixel)
    self.marginSizeMmLabel = self.scriptedEffect.addLabeledOptionsWidget("Margin size:", marginSizeFrame)
    self.scriptedEffect.addOptionsWidget(marginSizeFrame)

    # Connections
    self.marginSizeMmSpinBox.connect("valueChanged(double)", self.updateMRMLFromGUI)

  def setMRMLDefaults(self):
    self.scriptedEffect.setParameterDefault("MarginSizeMm", 3)
    self.scriptedEffect.setParameterDefault(HISTOGRAM_BRUSH_TYPE_PARAMETER_NAME, HISTOGRAM_BRUSH_TYPE_DRAW)
    SegmentEditorThresholdEffect.setMRMLDefaults(self)

  def updateGUIFromMRML(self):
    SegmentEditorThresholdEffect.updateGUIFromMRML(self)

    marginSizeMm = self.scriptedEffect.doubleParameter("MarginSizeMm")
    wasBlocked = self.marginSizeMmSpinBox.blockSignals(True)
    self.marginSizeMmSpinBox.value = abs(marginSizeMm)
    self.marginSizeMmSpinBox.blockSignals(wasBlocked)


  def updateMRMLFromGUI(self):
    SegmentEditorThresholdEffect.updateMRMLFromGUI(self)
    marginSizeMm = self.marginSizeMmSpinBox.value
    self.scriptedEffect.setParameter("MarginSizeMm", marginSizeMm)

  def processInteractionEvents(self, callerInteractor, eventId, viewWidget):
    abortEvent = False

    if not callerInteractor.GetControlKey():
      return SegmentEditorThresholdEffect.processInteractionEvents(self, callerInteractor, eventId, viewWidget)

    if eventId == vtk.vtkCommand.LeftButtonPressEvent:
      abortEvent = True

      masterImageData = self.scriptedEffect.masterVolumeImageData()

      xy = callerInteractor.GetEventPosition()
      ijk = self.xyToIjk(xy, viewWidget, masterImageData)

      ijkPoints = vtk.vtkPoints() # TODO: Could get points from all of preview
      origin = masterImageData.GetOrigin()
      spacing = masterImageData.GetSpacing()
      ijkPoints.InsertNextPoint(origin[0]+ijk[0]*spacing[0], origin[1]+ijk[1]*spacing[1], origin[2]+ijk[2]*spacing[2])
      self.keepIslandAtPoints(ijkPoints)

    return abortEvent

  def keepIslandAtPoints(self, ijkPoints):
    kernelSizePixel = self.getKernelSizePixel()
    if kernelSizePixel[0]<=1 and kernelSizePixel[1]<=1 and kernelSizePixel[2]<=1:
      return

    # Get parameters
    minimumThreshold = self.scriptedEffect.doubleParameter("MinimumThreshold")
    maximumThreshold = self.scriptedEffect.doubleParameter("MaximumThreshold")

    # Get modifier labelmap
    modifierLabelmap = self.scriptedEffect.defaultModifierLabelmap()

    try:
      # Get master volume image data
      import vtkSegmentationCorePython as vtkSegmentationCore
      masterImageData = self.scriptedEffect.masterVolumeImageData()

      # Perform thresholding
      self.thresh.SetInputData(masterImageData)
      self.thresh.ThresholdBetween(minimumThreshold, maximumThreshold)
      self.thresh.SetOutputScalarType(modifierLabelmap.GetScalarType())
      self.thresh.Update()

    except IndexError:
      logging.error('apply: Failed to threshold master volume!')
      return
      pass

    parameterSetNode = self.scriptedEffect.parameterSetNode()

    oldMasterVolumeIntensityMask = parameterSetNode.GetMasterVolumeIntensityMask()
    parameterSetNode.MasterVolumeIntensityMaskOn()

    oldIntensityMaskRange = parameterSetNode.GetMasterVolumeIntensityMaskRange()
    intensityRange = [minimumThreshold, maximumThreshold]
    if oldMasterVolumeIntensityMask:
      intensityRange = [max(oldIntensityMaskRange[0], minimumThreshold), min(oldIntensityMaskRange[1], maximumThreshold)]
    parameterSetNode.SetMasterVolumeIntensityMaskRange(intensityRange)

    self.erode.SetKernelSize(kernelSizePixel[0],kernelSizePixel[1],kernelSizePixel[2])
    self.floodFillingFilter.SetSeedPoints(ijkPoints)
    self.dilate.SetKernelSize(kernelSizePixel[0],kernelSizePixel[1],kernelSizePixel[2])
    self.dilate.Update()

    modifierLabelmap.ShallowCopy(self.dilate.GetOutput())

    self.scriptedEffect.saveStateForUndo()
    self.scriptedEffect.modifySelectedSegmentByLabelmap(modifierLabelmap, slicer.qSlicerSegmentEditorAbstractEffect.ModificationModeAdd)

    parameterSetNode.SetMasterVolumeIntensityMask(oldMasterVolumeIntensityMask)
    parameterSetNode.SetMasterVolumeIntensityMaskRange(oldIntensityMaskRange)


  def getKernelSizePixel(self):
    selectedSegmentLabelmapSpacing = [1.0, 1.0, 1.0]
    selectedSegmentLabelmap = self.scriptedEffect.selectedSegmentLabelmap()
    if selectedSegmentLabelmap:
      selectedSegmentLabelmapSpacing = selectedSegmentLabelmap.GetSpacing()

    # size rounded to nearest odd number. If kernel size is even then image gets shifted.
    marginSizeMm = abs(self.scriptedEffect.doubleParameter("MarginSizeMm"))
    kernelSizePixel = [int(round((marginSizeMm / selectedSegmentLabelmapSpacing[componentIndex]+1)/2)*2-1) for componentIndex in range(3)]
    return kernelSizePixel
