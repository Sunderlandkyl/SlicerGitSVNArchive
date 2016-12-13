import os
import vtk, qt, ctk, slicer, math
import logging
from SegmentEditorEffects import *

class SegmentEditorThresholdEffect(AbstractScriptedSegmentEditorEffect):
  """ ThresholdEffect is an Effect implementing the global threshold
      operation in the segment editor

      This is also an example for scripted effects, and some methods have no
      function. The methods that are not needed (i.e. the default implementation in
      qSlicerSegmentEditorAbstractEffect is satisfactory) can simply be omitted.
  """

  def __init__(self, scriptedEffect):
    AbstractScriptedSegmentEditorEffect.__init__(self, scriptedEffect)
    scriptedEffect.name = 'Threshold'

    # Effect-specific members
    self.timer = qt.QTimer()
    self.previewState = 0
    self.previewStep = 1
    self.previewSteps = 5
    self.timer.connect('timeout()', self.preview)

    self.previewPipelines = {}
    self.setupPreviewDisplay()

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

  def activate(self):
    # Save segment opacity and set it to zero
    segmentationNode = self.scriptedEffect.parameterSetNode().GetSegmentationNode()
    segmentID = self.scriptedEffect.parameterSetNode().GetSelectedSegmentID()
    displayNode = segmentationNode.GetDisplayNode()
    if displayNode is not None:
      self.segment2DFillOpacity = displayNode.GetSegmentOpacity2DFill(segmentID)
      self.segment2DOutlineOpacity = displayNode.GetSegmentOpacity2DOutline(segmentID)
      displayNode.SetSegmentOpacity2DFill(segmentID, 0)
      displayNode.SetSegmentOpacity2DOutline(segmentID, 0)

    # Update intensity range
    self.masterVolumeNodeChanged()

    # Setup and start preview pulse
    self.setupPreviewDisplay()
    self.timer.start(200)

  def deactivate(self):
    # Restore segment opacity
    segmentationNode = self.scriptedEffect.parameterSetNode().GetSegmentationNode()
    segmentID = self.scriptedEffect.parameterSetNode().GetSelectedSegmentID()
    displayNode = segmentationNode.GetDisplayNode()
    if (displayNode is not None) and (segmentID is not None):
      displayNode.SetSegmentOpacity2DFill(segmentID, self.segment2DFillOpacity)
      displayNode.SetSegmentOpacity2DOutline(segmentID, self.segment2DOutlineOpacity)

    # Clear preview pipeline and stop timer
    self.clearPreviewDisplay()
    self.timer.stop()

  def setupOptionsFrame(self):
    self.thresholdSliderLabel = qt.QLabel("Threshold Range:")
    self.thresholdSliderLabel.setToolTip("Set the range of the background values that should be labeled.")
    self.scriptedEffect.addOptionsWidget(self.thresholdSliderLabel)

    self.thresholdSlider = ctk.ctkRangeWidget()
    self.thresholdSlider.spinBoxAlignment = qt.Qt.AlignTop
    self.thresholdSlider.singleStep = 0.01
    self.scriptedEffect.addOptionsWidget(self.thresholdSlider)

    self.useForPaintButton = qt.QPushButton("Use for masking")
    self.useForPaintButton.setToolTip("Use specified intensity range for masking and switch to Paint effect.")
    self.scriptedEffect.addOptionsWidget(self.useForPaintButton)

    self.applyButton = qt.QPushButton("Apply")
    self.applyButton.objectName = self.__class__.__name__ + 'Apply'
    self.applyButton.setToolTip("Fill selected segment in regions that are in the specified intensity range.")
    self.scriptedEffect.addOptionsWidget(self.applyButton)

    self.useForPaintButton.connect('clicked()', self.onUseForPaint)
    self.thresholdSlider.connect('valuesChanged(double,double)', self.onThresholdValuesChanged)
    self.applyButton.connect('clicked()', self.onApply)

  def createCursor(self, widget):
    # Turn off effect-specific cursor for this effect
    return slicer.util.mainWindow().cursor

  def masterVolumeNodeChanged(self):
    # Set scalar range of master volume image data to threshold slider
    import vtkSegmentationCorePython as vtkSegmentationCore
    masterImageData = self.scriptedEffect.masterVolumeImageData()
    if masterImageData:
      lo, hi = masterImageData.GetScalarRange()
      self.thresholdSlider.minimum, self.thresholdSlider.maximum = lo, hi
      self.thresholdSlider.singleStep = (hi - lo) / 1000.
      if (self.scriptedEffect.doubleParameter("MinimumThreshold") == self.scriptedEffect.doubleParameter("MaximumThreshold")):
        # has not been initialized yet
        self.scriptedEffect.setParameter("MinimumThreshold", lo+(hi-lo)*0.25)
        self.scriptedEffect.setParameter("MaximumThreshold", hi)

  def layoutChanged(self):
    self.setupPreviewDisplay()

  def processInteractionEvents(self, callerInteractor, eventId, viewWidget):
    return False # For the sake of example

  def processViewNodeEvents(self, callerViewNode, eventId, viewWidget):
    pass # For the sake of example

  def setMRMLDefaults(self):
    self.scriptedEffect.setParameterDefault("MinimumThreshold", 0.)
    self.scriptedEffect.setParameterDefault("MaximumThreshold", 0)

  def updateGUIFromMRML(self):
    self.thresholdSlider.blockSignals(True)
    self.thresholdSlider.setMinimumValue(self.scriptedEffect.doubleParameter("MinimumThreshold"))
    self.thresholdSlider.setMaximumValue(self.scriptedEffect.doubleParameter("MaximumThreshold"))
    self.thresholdSlider.blockSignals(False)

  def updateMRMLFromGUI(self):
    self.scriptedEffect.setParameter("MinimumThreshold", self.thresholdSlider.minimumValue)
    self.scriptedEffect.setParameter("MaximumThreshold", self.thresholdSlider.maximumValue)

  #
  # Effect specific methods (the above ones are the API methods to override)
  #
  def onThresholdValuesChanged(self,min,max):
    self.scriptedEffect.updateMRMLFromGUI()

  def onUseForPaint(self):
    parameterSetNode = self.scriptedEffect.parameterSetNode()
    parameterSetNode.MasterVolumeIntensityMaskOn()
    parameterSetNode.SetMasterVolumeIntensityMaskRange(self.thresholdSlider.minimumValue, self.thresholdSlider.maximumValue)
    # Switch to paint effect
    self.scriptedEffect.selectEffect("Paint")

  def onApply(self):
    try:
      # Get master volume image data
      import vtkSegmentationCorePython as vtkSegmentationCore
      masterImageData = self.scriptedEffect.masterVolumeImageData()
      # Get modifier labelmap
      modifierLabelmap = self.scriptedEffect.defaultModifierLabelmap()
      originalImageToWorldMatrix = vtk.vtkMatrix4x4()
      modifierLabelmap.GetImageToWorldMatrix(originalImageToWorldMatrix)
      originalScalarType = modifierLabelmap.GetScalarType()
      originalExtent = modifierLabelmap.GetExtent()
      # Get parameters
      minThreshold = self.scriptedEffect.doubleParameter("MinimumThreshold")
      maxThreshold = self.scriptedEffect.doubleParameter("MaximumThreshold")

      self.scriptedEffect.saveStateForUndo()

      segmentation = self.scriptedEffect.parameterSetNode().GetSegmentationNode().GetSegmentation()

      # Perform thresholding
      thresh = vtk.vtkImageThreshold()
      thresh.SetInputData(masterImageData)
      thresh.ThresholdBetween(minThreshold, maxThreshold)
      thresh.SetInValue(1)
      thresh.SetOutValue(0)
      thresh.SetOutputScalarType(modifierLabelmap.GetScalarType())

      masterRepresentationIsFractionalLabelmap = segmentation.GetMasterRepresentationName() == vtkSegmentationCore.vtkSegmentationConverter.GetSegmentationFractionalLabelmapRepresentationName()

      if (masterRepresentationIsFractionalLabelmap):
        scalarRange = [-108.0, 108.0]
        thresholdValue = 0.0
        interpolationType = vtk.VTK_LINEAR_INTERPOLATION
        scalarType = vtk.VTK_CHAR

        fractionalLabelmapTemplate = segmentation.GetSegmentRepresentation(
          segmentation.GetNthSegmentID(0), vtkSegmentationCore.vtkSegmentationConverter.GetSegmentationFractionalLabelmapRepresentationName())
        fractionalExtent = {0, -1, 0, -1, 0, -1}
        fractionalExtent = fractionalLabelmapTemplate.GetExtent()

        if (fractionalLabelmapTemplate and
            fractionalExtent[1] > fractionalExtent[0] or
            fractionalExtent[3] > fractionalExtent[2] or
            fractionalExtent[5] > fractionalExtent[4] ):

            scalarRangeArray = vtk.vtkDoubleArray.SafeDownCast(
              fractionalLabelmapTemplate.GetFieldData().GetAbstractArray(vtkSegmentationCore.vtkSegmentationConverter.GetScalarRangeFieldName()))
            if (scalarRangeArray and scalarRangeArray.GetNumberOfValues() == 2 ):
              scalarRange[0] = scalarRangeArray.GetValue(0)
              scalarRange[1] = scalarRangeArray.GetValue(1)

            thresholdValueArray = vtk.vtkDoubleArray.SafeDownCast(
              fractionalLabelmapTemplate.GetFieldData().GetAbstractArray(vtkSegmentationCore.vtkSegmentationConverter.GetThresholdValueFieldName()))
            if (thresholdValueArray and thresholdValueArray.GetNumberOfValues() == 1 ):
              threshold = thresholdValueArray.GetValue(0)

            interpolationTypeArray = vtk.vtkIntArray.SafeDownCast(
              fractionalLabelmapTemplate.GetFieldData().GetAbstractArray(vtkSegmentationCore.vtkSegmentationConverter.GetThresholdValueFieldName()))
            if (interpolationTypeArray and interpolationTypeArray.GetNumberOfValues() == 1 ):
              interpolationType = interpolationTypeArray.GetValue(0)

            scalarType = fractionalLabelmapTemplate.GetScalarType()

        origin = masterImageData.GetOrigin()
        dimensions = masterImageData.GetDimensions()
        spacing = masterImageData.GetSpacing()
        extent = masterImageData.GetExtent()

        imageToWorldMatrix = vtk.vtkMatrix4x4()
        masterImageData.GetImageToWorldMatrix(imageToWorldMatrix)

        oversamplingFactor = 6.0
        shift =  (oversamplingFactor-1.0)/(2.0*oversamplingFactor)

        # TODO: determine if offsets are calculated correctly
        #offsetOrigin = imageToWorldMatrix.MultiplyDoublePoint([extent[0]-shift, extent[2]-shift, extent[4]-shift, 1]) # does not work
        #offsetOrigin = imageToWorldMatrix.MultiplyDoublePoint([extent[0]+shift, extent[2]+shift, extent[4]-shift, 1]) # works
        #offsetOrigin = offsetOrigin[0:3]
        offsetOrigin = [origin[0] + (extent[0] - shift) * spacing[0],
                        origin[1] + (extent[2] - shift) * spacing[1],
                        origin[2] + (extent[4] - shift) * spacing[2]]

        modifierLabelmap.SetImageToWorldMatrix(imageToWorldMatrix)
        modifierLabelmap.SetExtent(extent)
        modifierLabelmap.AllocateScalars(scalarType, 1)

        fill = vtk.vtkImageThreshold()
        fill.SetInputData(modifierLabelmap)
        fill.ThresholdBetween(0,0)
        fill.SetInValue(scalarRange[0])
        fill.SetOutValue(scalarRange[0])
        fill.SetOutputScalarType(scalarType)
        fill.Update()
        modifierLabelmap.DeepCopy(fill.GetOutput())

        reslice = vtk.vtkImageReslice()
        reslice.SetInputData(masterImageData)
        reslice.SetInterpolationModeToLinear()
        reslice.SetOutputOrigin(offsetOrigin)
        reslice.SetOutputSpacing(spacing[0]/6, spacing[1]/6, spacing[2]/6)

        thresh.SetInputData(None)
        thresh.SetInputConnection(reslice.GetOutputPort())

        stepSize = [int(math.ceil(dimensions[0]/oversamplingFactor)),
                    int(math.ceil(dimensions[1]/oversamplingFactor)),
                    int(math.ceil(dimensions[2]/oversamplingFactor))]

        for k in range(0, dimensions[2], stepSize[2]):
          for j in range(0, dimensions[1], stepSize[1]):
            for i in range(0, dimensions[0], stepSize[0]):

              offsetExtent = [
                int( i * oversamplingFactor),
                int( min( i * oversamplingFactor + stepSize[0] * oversamplingFactor - 1, dimensions[0] * oversamplingFactor - 1)),
                int( j * oversamplingFactor),
                int( min( j * oversamplingFactor + stepSize[1] * oversamplingFactor - 1, dimensions[1] * oversamplingFactor - 1)),
                int( k * oversamplingFactor),
                int( min( k * oversamplingFactor + stepSize[2] * oversamplingFactor - 1, dimensions[2] * oversamplingFactor - 1))
                ]

              fractionalExtent = [
                i, min(i + stepSize[0] - 1, dimensions[0] - 1),
                j, min(j + stepSize[1] - 1, dimensions[1] - 1),
                k, min(k + stepSize[2] - 1, dimensions[2] - 1)
                ]

              reslice.SetOutputExtent(offsetExtent)
              thresh.Update()

              resampleBinaryToFractionalFilter = vtkSegmentationCore.vtkResampleBinaryLabelmapToFractionalLabelmap()
              resampleBinaryToFractionalFilter.SetInputConnection(thresh.GetOutputPort())
              resampleBinaryToFractionalFilter.SetOutputScalarType(scalarType)
              resampleBinaryToFractionalFilter.SetOutputMinimumValue(scalarRange[0])
              resampleBinaryToFractionalFilter.SetOutputExtent(fractionalExtent)
              resampleBinaryToFractionalFilter.Update()

              fractionalLabelmap = vtkSegmentationCore.vtkOrientedImageData()
              fractionalLabelmap.ShallowCopy(resampleBinaryToFractionalFilter.GetOutput())

              #matrix = vtk.vtkMatrix4x4()
              #modifierLabelmap.GetImageToWorldMatrix(matrix)
              fractionalLabelmap.SetImageToWorldMatrix(imageToWorldMatrix)
              vtkSegmentationCore.vtkOrientedImageDataResample.MergeImage(modifierLabelmap, fractionalLabelmap, modifierLabelmap, vtkSegmentationCore.vtkOrientedImageDataResample.OPERATION_MAXIMUM)

        # Specify the scalar range of values in the labelmap
        scalarRangeArray = vtk.vtkDoubleArray()
        scalarRangeArray.SetName(vtkSegmentationCore.vtkSegmentationConverter.GetScalarRangeFieldName())
        scalarRangeArray.InsertNextValue(scalarRange[0])
        scalarRangeArray.InsertNextValue(scalarRange[1])
        modifierLabelmap.GetFieldData().AddArray(scalarRangeArray)

        # Specify the surface threshold value for visualization
        thresholdValueArray = vtk.vtkDoubleArray()
        thresholdValueArray.SetName(vtkSegmentationCore.vtkSegmentationConverter.GetThresholdValueFieldName())
        thresholdValueArray.InsertNextValue(thresholdValue)
        modifierLabelmap.GetFieldData().AddArray(thresholdValueArray)

        # Specify the interpolation type for visualization
        interpolationTypeArray = vtk.vtkIntArray()
        interpolationTypeArray.SetName(vtkSegmentationCore.vtkSegmentationConverter.GetInterpolationTypeFieldName())
        interpolationTypeArray.InsertNextValue(interpolationType)
        modifierLabelmap.GetFieldData().AddArray(interpolationTypeArray)

      else:
        thresh.Update()
        modifierLabelmap.DeepCopy(thresh.GetOutput())
    except IndexError:
      logging.error('apply: Failed to threshold master volume!')
      pass

    # Apply changes
    self.scriptedEffect.modifySelectedSegmentByLabelmap(modifierLabelmap, slicer.qSlicerSegmentEditorAbstractEffect.ModificationModeSet)

    # TODO: should the modifier labelmap just be copied at the start instead of resetting it?
    # Reset modifier labelmap
    modifierLabelmap.SetImageToWorldMatrix(originalImageToWorldMatrix)
    modifierLabelmap.SetExtent(originalExtent)
    if (not modifierLabelmap.GetScalarType() == originalScalarType):
      modifierLabelmap.AllocateScalars(originalScalarType, 1)

    # De-select effect
    self.scriptedEffect.selectEffect("")

  def clearPreviewDisplay(self):
    for sliceWidget, pipeline in self.previewPipelines.iteritems():
      self.scriptedEffect.removeActor2D(sliceWidget, pipeline.actor)
    self.previewPipelines = {}

  def setupPreviewDisplay(self):
    # Clear previous pipelines before setting up the new ones
    self.clearPreviewDisplay()

    layoutManager = slicer.app.layoutManager()
    if layoutManager is None:
      return

    # Add a pipeline for each 2D slice view
    for sliceViewName in layoutManager.sliceViewNames():
      sliceWidget = layoutManager.sliceWidget(sliceViewName)
      renderer = self.scriptedEffect.renderer(sliceWidget)
      if renderer is None:
        logging.error("setupPreviewDisplay: Failed to get renderer!")
        continue

      # Create pipeline
      pipeline = PreviewPipeline()
      self.previewPipelines[sliceWidget] = pipeline

      # Add actor
      self.scriptedEffect.addActor2D(sliceWidget, pipeline.actor)

  def preview(self):
    opacity = 0.5 + self.previewState / (2. * self.previewSteps)
    min = self.scriptedEffect.doubleParameter("MinimumThreshold")
    max = self.scriptedEffect.doubleParameter("MaximumThreshold")

    # Get color of edited segment
    segmentationNode = self.scriptedEffect.parameterSetNode().GetSegmentationNode()
    displayNode = segmentationNode.GetDisplayNode()
    if displayNode is None:
      logging.error("preview: Invalid segmentation display node!")
      color = [0.5,0.5,0.5]
    segmentID = self.scriptedEffect.parameterSetNode().GetSelectedSegmentID()
    r,g,b = segmentationNode.GetSegmentation().GetSegment(segmentID).GetColor()

    # Set values to pipelines
    for sliceWidget in self.previewPipelines:
      pipeline = self.previewPipelines[sliceWidget]
      pipeline.lookupTable.SetTableValue(1,  r, g, b,  opacity)
      sliceLogic = sliceWidget.sliceLogic()
      backgroundLogic = sliceLogic.GetBackgroundLayer()
      pipeline.thresholdFilter.SetInputConnection(backgroundLogic.GetReslice().GetOutputPort())
      pipeline.thresholdFilter.ThresholdBetween(min, max)
      pipeline.actor.VisibilityOn()
      sliceWidget.sliceView().scheduleRender()

    self.previewState += self.previewStep
    if self.previewState >= self.previewSteps:
      self.previewStep = -1
    if self.previewState <= 0:
      self.previewStep = 1

#
# PreviewPipeline
#
class PreviewPipeline:
  """ Visualization objects and pipeline for each slice view for threshold preview
  """

  def __init__(self):
    self.lookupTable = vtk.vtkLookupTable()
    self.lookupTable.SetRampToLinear()
    self.lookupTable.SetNumberOfTableValues(2)
    self.lookupTable.SetTableRange(0, 1)
    self.lookupTable.SetTableValue(0,  0, 0, 0,  0)
    self.colorMapper = vtk.vtkImageMapToRGBA()
    self.colorMapper.SetOutputFormatToRGBA()
    self.colorMapper.SetLookupTable(self.lookupTable)
    self.thresholdFilter = vtk.vtkImageThreshold()
    self.thresholdFilter.SetInValue(1)
    self.thresholdFilter.SetOutValue(0)
    self.thresholdFilter.SetOutputScalarTypeToUnsignedChar()

    # Feedback actor
    self.mapper = vtk.vtkImageMapper()
    self.dummyImage = vtk.vtkImageData()
    self.dummyImage.AllocateScalars(vtk.VTK_UNSIGNED_INT, 1)
    self.mapper.SetInputData(self.dummyImage)
    self.actor = vtk.vtkActor2D()
    self.actor.VisibilityOff()
    self.actor.SetMapper(self.mapper)
    self.mapper.SetColorWindow(255)
    self.mapper.SetColorLevel(128)

    # Setup pipeline
    self.colorMapper.SetInputConnection(self.thresholdFilter.GetOutputPort())
    self.mapper.SetInputConnection(self.colorMapper.GetOutputPort())
