import os
import vtk, qt, ctk, slicer
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

    self.segment2DFillOpacity = None
    self.segment2DOutlineOpacity = None
    self.previewedSegmentID = None

    # Effect-specific members
    import vtkITK
    self.autoThresholdCalculator = vtkITK.vtkITKImageThresholdCalculator()

    self.timer = qt.QTimer()
    self.previewState = 0
    self.previewStep = 1
    self.previewSteps = 5
    self.timer.connect('timeout()', self.preview)

    self.previewPipelines = {}
    self.histogramPipeline = None
    self.setupPreviewDisplay()

    # Histogram stencil setup
    self.stencil = vtk.vtkPolyDataToImageStencil()

    # Histogram reslice setup
    self.reslice = vtk.vtkImageReslice()
    self.reslice.AutoCropOutputOff()
    self.reslice.SetOptimization(1)
    self.reslice.SetOutputOrigin(0, 0, 0)
    self.reslice.SetOutputSpacing(1, 1, 1)
    self.reslice.SetOutputDimensionality(3)
    self.reslice.GenerateStencilOutputOn()

    self.imageAccumulate = vtk.vtkImageAccumulate()
    self.imageAccumulate.SetInputConnection(0, self.reslice.GetOutputPort())
    self.imageAccumulate.SetInputConnection(1, self.stencil.GetOutputPort())

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
    self.setCurrentSegmentTransparent()

    # Update intensity range
    self.masterVolumeNodeChanged()

    # Setup and start preview pulse
    self.setupPreviewDisplay()
    self.timer.start(200)

  def deactivate(self):
    self.restorePreviewedSegmentTransparency()

    # Clear preview pipeline and stop timer
    self.clearPreviewDisplay()
    self.clearHistogramDisplay()
    self.timer.stop()

  def setCurrentSegmentTransparent(self):
    """Save current segment opacity and set it to zero
    to temporarily hide the segment so that threshold preview
    can be seen better.
    It also restores opacity of previously previewed segment.
    Call restorePreviewedSegmentTransparency() to restore original
    opacity.
    """
    segmentationNode = self.scriptedEffect.parameterSetNode().GetSegmentationNode()
    if not segmentationNode:
      return
    displayNode = segmentationNode.GetDisplayNode()
    if not displayNode:
      return
    segmentID = self.scriptedEffect.parameterSetNode().GetSelectedSegmentID()

    if segmentID == self.previewedSegmentID:
      # already previewing the current segment
      return

    # If an other segment was previewed before, restore that.
    if self.previewedSegmentID:
      self.restorePreviewedSegmentTransparency()

    # Make current segment fully transparent
    if segmentID:
      self.segment2DFillOpacity = displayNode.GetSegmentOpacity2DFill(segmentID)
      self.segment2DOutlineOpacity = displayNode.GetSegmentOpacity2DOutline(segmentID)
      self.previewedSegmentID = segmentID
      displayNode.SetSegmentOpacity2DFill(segmentID, 0)
      displayNode.SetSegmentOpacity2DOutline(segmentID, 0)

  def restorePreviewedSegmentTransparency(self):
    """Restore previewed segment's opacity that was temporarily
    made transparen by calling setCurrentSegmentTransparent()."""
    segmentationNode = self.scriptedEffect.parameterSetNode().GetSegmentationNode()
    if not segmentationNode:
      return
    displayNode = segmentationNode.GetDisplayNode()
    if not displayNode:
      return
    if not self.previewedSegmentID:
      # already previewing the current segment
      return
    displayNode.SetSegmentOpacity2DFill(self.previewedSegmentID, self.segment2DFillOpacity)
    displayNode.SetSegmentOpacity2DOutline(self.previewedSegmentID, self.segment2DOutlineOpacity)
    self.previewedSegmentID = None

  def setupOptionsFrame(self):
    self.thresholdSliderLabel = qt.QLabel("Threshold Range:")
    self.thresholdSliderLabel.setToolTip("Set the range of the background values that should be labeled.")
    self.scriptedEffect.addOptionsWidget(self.thresholdSliderLabel)

    self.thresholdSlider = ctk.ctkRangeWidget()
    self.thresholdSlider.spinBoxAlignment = qt.Qt.AlignTop
    self.thresholdSlider.singleStep = 0.01
    self.scriptedEffect.addOptionsWidget(self.thresholdSlider)

    self.autoThresholdModeSelectorComboBox = qt.QComboBox()
    self.autoThresholdModeSelectorComboBox.addItem("auto->maximum", MODE_SET_LOWER_MAX)
    self.autoThresholdModeSelectorComboBox.addItem("minimum->auto", MODE_SET_MIN_UPPER)
    self.autoThresholdModeSelectorComboBox.addItem("as lower", MODE_SET_LOWER)
    self.autoThresholdModeSelectorComboBox.addItem("as upper", MODE_SET_UPPER)
    self.autoThresholdModeSelectorComboBox.setToolTip("How to set lower and upper threshold values. Current refers to keeping the current value.")

    self.autoThresholdMethodSelectorComboBox = qt.QComboBox()
    self.autoThresholdMethodSelectorComboBox.addItem("Otsu", METHOD_OTSU)
    self.autoThresholdMethodSelectorComboBox.addItem("Huang", METHOD_HUANG)
    self.autoThresholdMethodSelectorComboBox.addItem("IsoData", METHOD_ISO_DATA)
    # Kittler-Illingworth sometimes fails with an exception, but it does not cause any major issue,
    # it just logs an error message and does not compute a new threshold value
    self.autoThresholdMethodSelectorComboBox.addItem("Kittler-Illingworth", METHOD_KITTLER_ILLINGWORTH)
    # Li sometimes crashes (index out of range error in
    # ITK/Modules/Filtering/Thresholding/include/itkLiThresholdCalculator.hxx#L94)
    # We can add this method back when issue is fixed in ITK.
    #self.autoThresholdMethodSelectorComboBox.addItem("Li", METHOD_LI)
    self.autoThresholdMethodSelectorComboBox.addItem("Maximum entropy", METHOD_MAXIMUM_ENTROPY)
    self.autoThresholdMethodSelectorComboBox.addItem("Moments", METHOD_MOMENTS)
    self.autoThresholdMethodSelectorComboBox.addItem("Renyi entropy", METHOD_RENYI_ENTROPY)
    self.autoThresholdMethodSelectorComboBox.addItem("Shanbhag", METHOD_SHANBHAG)
    self.autoThresholdMethodSelectorComboBox.addItem("Triangle", METHOD_TRIANGLE)
    self.autoThresholdMethodSelectorComboBox.addItem("Yen", METHOD_YEN)
    self.autoThresholdMethodSelectorComboBox.setToolTip("Select method to compute threshold value automatically.")

    self.selectPreviousAutoThresholdButton = qt.QToolButton()
    self.selectPreviousAutoThresholdButton.text = "<"
    self.selectPreviousAutoThresholdButton.setToolTip("Select previous thresholding method and set thresholds."
      +" Useful for iterating through all available methods.")

    self.selectNextAutoThresholdButton = qt.QToolButton()
    self.selectNextAutoThresholdButton.text = ">"
    self.selectNextAutoThresholdButton.setToolTip("Select next thresholding method and set thresholds."
      +" Useful for iterating through all available methods.")

    self.setAutoThresholdButton = qt.QPushButton("Set")
    self.setAutoThresholdButton.setToolTip("Set threshold using selected method.")

    # qt.QSizePolicy(qt.QSizePolicy.Expanding, qt.QSizePolicy.Expanding)
    # fails on some systems, therefore set the policies using separate method calls
    qSize = qt.QSizePolicy()
    qSize.setHorizontalPolicy(qt.QSizePolicy.Expanding)
    self.setAutoThresholdButton.setSizePolicy(qSize)

    autoThresholdFrame = qt.QHBoxLayout()
    autoThresholdFrame.addWidget(self.autoThresholdModeSelectorComboBox)
    autoThresholdFrame.addWidget(self.autoThresholdMethodSelectorComboBox)
    autoThresholdFrame.addWidget(self.selectPreviousAutoThresholdButton)
    autoThresholdFrame.addWidget(self.selectNextAutoThresholdButton)
    autoThresholdFrame.addWidget(self.setAutoThresholdButton)

    autoThresholdGroupBox = ctk.ctkCollapsibleGroupBox()
    autoThresholdGroupBox.setTitle("Automatic threshold")
    autoThresholdGroupBox.setLayout(autoThresholdFrame)
    autoThresholdGroupBox.collapsed = True
    self.scriptedEffect.addOptionsWidget(autoThresholdGroupBox)

    histogramFrame = qt.QVBoxLayout()

    histogramBrushFrame = qt.QHBoxLayout()
    histogramFrame.addLayout(histogramBrushFrame)

    #self.lineROIButton = qt.QPushButton()
    #self.lineROIButton.setText("Line")
    #histogramBrushFrame.addWidget(self.lineROIButton)

    self.boxROIButton = qt.QPushButton()
    self.boxROIButton.setText("Box")
    self.boxROIButton.setCheckable(True)
    self.boxROIButton.checked = False
    self.boxROIButton.clicked.connect(self.setBrushToBox)
    histogramBrushFrame.addWidget(self.boxROIButton)

    self.circleROIButton = qt.QPushButton()
    self.circleROIButton.setText("Circle")
    self.circleROIButton.setCheckable(True)
    self.circleROIButton.checked = True
    self.circleROIButton.clicked.connect(self.setBrushToCircle)
    histogramBrushFrame.addWidget(self.circleROIButton)

    self.histogramBrushButtonGroup = qt.QButtonGroup()
    self.histogramBrushButtonGroup.addButton(self.boxROIButton)
    self.histogramBrushButtonGroup.addButton(self.circleROIButton)
    self.histogramBrushButtonGroup.setExclusive(True)

    self.histogramView = ctk.ctkTransferFunctionView()
    histogramFrame.addWidget(self.histogramView)
    scene = self.histogramView.scene()

    self.histogramFunction = vtk.vtkPiecewiseFunction()
    self.histogramFunctionContainer = ctk.ctkVTKPiecewiseFunction(self.scriptedEffect)
    self.histogramFunctionContainer.setPiecewiseFunction(self.histogramFunction)
    self.histogramBars = ctk.ctkTransferFunctionBarsItem(self.histogramFunctionContainer)
    self.histogramBars.barWidth = 1.0
    self.histogramBars.logMode = ctk.ctkTransferFunctionBarsItem.NoLog
    self.histogramBars.transferFunctionMousePressed.connect(self.onHistogramMouseClick)
    self.histogramBars.transferFunctionMouseMove.connect(self.onHistogramMouseMove)
    self.histogramBars.transferFunctionMouseReleased.connect(self.onHistogramMouseRelease)
    self.histogramBars.setZValue(1);
    scene.addItem(self.histogramBars)

    self.selectionFunction = vtk.vtkPiecewiseFunction()
    self.selectionFunctionContainer = ctk.ctkVTKPiecewiseFunction(self.scriptedEffect)
    self.selectionFunctionContainer.setPiecewiseFunction(self.selectionFunction)
    self.selectionBars = ctk.ctkTransferFunctionBarsItem(self.selectionFunctionContainer)
    self.selectionBars.barWidth = 0.025
    self.selectionBars.logMode = ctk.ctkTransferFunctionBarsItem.NoLog
    self.selectionBars.barColor = qt.QColor(200, 0, 0)
    self.selectionBars.setZValue(0);
    scene.addItem(self.selectionBars)

    self.averageFunction = vtk.vtkPiecewiseFunction()
    self.averageFunctionContainer = ctk.ctkVTKPiecewiseFunction(self.scriptedEffect)
    self.averageFunctionContainer.setPiecewiseFunction(self.averageFunction)
    self.averageBars = ctk.ctkTransferFunctionBarsItem(self.averageFunctionContainer)
    self.averageBars.barWidth = 0.04
    self.averageBars.logMode = ctk.ctkTransferFunctionBarsItem.NoLog
    self.averageBars.barColor = qt.QColor(255, 200, 0)
    self.averageBars.setZValue(-1);
    scene.addItem(self.averageBars)

    histogramItemFrame = qt.QHBoxLayout()
    histogramFrame.addLayout(histogramItemFrame)

    self.histogramMethodButtonGroup = qt.QButtonGroup()

    self.lowerHistogramButton = qt.QPushButton()
    self.lowerHistogramButton.setText("Lower")
    self.lowerHistogramButton.setCheckable(True)
    self.lowerHistogramButton.checked = False
    self.lowerHistogramButton.clicked.connect(self.setHistogramToLower)
    histogramItemFrame.addWidget(self.lowerHistogramButton)
    self.histogramMethodButtonGroup.addButton(self.lowerHistogramButton)

    self.rangeHistogramButton = qt.QPushButton()
    self.rangeHistogramButton.setText("Full range")
    self.rangeHistogramButton.setCheckable(True)
    self.rangeHistogramButton.checked = True
    self.rangeHistogramButton.clicked.connect(self.setHistogramToRange)
    histogramItemFrame.addWidget(self.rangeHistogramButton)
    self.histogramMethodButtonGroup.addButton(self.rangeHistogramButton)

    self.upperHistogramButton = qt.QPushButton()
    self.upperHistogramButton.setText("Upper")
    self.upperHistogramButton.setCheckable(True)
    self.upperHistogramButton.checked = False
    self.upperHistogramButton.clicked.connect(self.setHistogramToUpper)
    histogramItemFrame.addWidget(self.upperHistogramButton)
    self.histogramMethodButtonGroup.addButton(self.upperHistogramButton)

    self.histogramMethodButtonGroup.setExclusive(True)

    histogramGroupBox = ctk.ctkCollapsibleGroupBox()
    histogramGroupBox.setTitle("Local histogram")
    histogramGroupBox.setLayout(histogramFrame)
    histogramGroupBox.collapsed = True
    self.scriptedEffect.addOptionsWidget(histogramGroupBox)

    self.useForPaintButton = qt.QPushButton("Use for masking")
    self.useForPaintButton.setToolTip("Use specified intensity range for masking and switch to Paint effect.")
    self.scriptedEffect.addOptionsWidget(self.useForPaintButton)

    self.applyButton = qt.QPushButton("Apply")
    self.applyButton.objectName = self.__class__.__name__ + 'Apply'
    self.applyButton.setToolTip("Fill selected segment in regions that are in the specified intensity range.")
    self.scriptedEffect.addOptionsWidget(self.applyButton)

    self.useForPaintButton.connect('clicked()', self.onUseForPaint)
    self.thresholdSlider.connect('valuesChanged(double,double)', self.onThresholdValuesChanged)
    self.autoThresholdMethodSelectorComboBox.connect("activated(int)", self.onSelectedAutoThresholdMethod)
    self.autoThresholdModeSelectorComboBox.connect("activated(int)", self.onSelectedAutoThresholdMethod)
    self.selectPreviousAutoThresholdButton.connect('clicked()', self.onSelectPreviousAutoThresholdMethod)
    self.selectNextAutoThresholdButton.connect('clicked()', self.onSelectNextAutoThresholdMethod)
    self.setAutoThresholdButton.connect('clicked()', self.onAutoThreshold)
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
      self.thresholdSlider.setRange(lo, hi)
      self.thresholdSlider.singleStep = (hi - lo) / 1000.
      if (self.scriptedEffect.doubleParameter("MinimumThreshold") == self.scriptedEffect.doubleParameter("MaximumThreshold")):
        # has not been initialized yet
        self.scriptedEffect.setParameter("MinimumThreshold", lo+(hi-lo)*0.25)
        self.scriptedEffect.setParameter("MaximumThreshold", hi)

  def layoutChanged(self):
    self.setupPreviewDisplay()

  def setMRMLDefaults(self):
    self.scriptedEffect.setParameterDefault("MinimumThreshold", 0.)
    self.scriptedEffect.setParameterDefault("MaximumThreshold", 0)
    self.scriptedEffect.setParameterDefault("AutoThresholdMethod", METHOD_OTSU)
    self.scriptedEffect.setParameterDefault("AutoThresholdMode", MODE_SET_LOWER_MAX)

  def updateGUIFromMRML(self):
    self.thresholdSlider.blockSignals(True)
    self.thresholdSlider.setMinimumValue(self.scriptedEffect.doubleParameter("MinimumThreshold"))
    self.thresholdSlider.setMaximumValue(self.scriptedEffect.doubleParameter("MaximumThreshold"))
    self.thresholdSlider.blockSignals(False)

    autoThresholdMethod = self.autoThresholdMethodSelectorComboBox.findData(self.scriptedEffect.parameter("AutoThresholdMethod"))
    wasBlocked = self.autoThresholdMethodSelectorComboBox.blockSignals(True)
    self.autoThresholdMethodSelectorComboBox.setCurrentIndex(autoThresholdMethod)
    self.autoThresholdMethodSelectorComboBox.blockSignals(wasBlocked)

    autoThresholdMode = self.autoThresholdModeSelectorComboBox.findData(self.scriptedEffect.parameter("AutoThresholdMode"))
    wasBlocked = self.autoThresholdModeSelectorComboBox.blockSignals(True)
    self.autoThresholdModeSelectorComboBox.setCurrentIndex(autoThresholdMode)
    self.autoThresholdModeSelectorComboBox.blockSignals(wasBlocked)

  def updateMRMLFromGUI(self):
    self.scriptedEffect.setParameter("MinimumThreshold", self.thresholdSlider.minimumValue)
    self.scriptedEffect.setParameter("MaximumThreshold", self.thresholdSlider.maximumValue)

    methodIndex = self.autoThresholdMethodSelectorComboBox.currentIndex
    autoThresholdMethod = self.autoThresholdMethodSelectorComboBox.itemData(methodIndex)
    self.scriptedEffect.setParameter("AutoThresholdMethod", autoThresholdMethod)

    modeIndex = self.autoThresholdModeSelectorComboBox.currentIndex
    autoThresholdMode = self.autoThresholdModeSelectorComboBox.itemData(modeIndex)
    self.scriptedEffect.setParameter("AutoThresholdMode", autoThresholdMode)

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

  def onSelectPreviousAutoThresholdMethod(self):
    self.autoThresholdMethodSelectorComboBox.currentIndex = (self.autoThresholdMethodSelectorComboBox.currentIndex - 1) \
      % self.autoThresholdMethodSelectorComboBox.count
    self.onSelectedAutoThresholdMethod()

  def onSelectNextAutoThresholdMethod(self):
    self.autoThresholdMethodSelectorComboBox.currentIndex = (self.autoThresholdMethodSelectorComboBox.currentIndex + 1) \
      % self.autoThresholdMethodSelectorComboBox.count
    self.onSelectedAutoThresholdMethod()

  def onSelectedAutoThresholdMethod(self):
    self.updateMRMLFromGUI()
    self.onAutoThreshold()
    self.updateGUIFromMRML()

  def onAutoThreshold(self):
    autoThresholdMethod = self.scriptedEffect.parameter("AutoThresholdMethod")
    autoThresholdMode = self.scriptedEffect.parameter("AutoThresholdMode")
    self.autoThreshold(autoThresholdMethod, autoThresholdMode)

  def autoThreshold(self, autoThresholdMethod, autoThresholdMode):
    if autoThresholdMethod == METHOD_HUANG:
      self.autoThresholdCalculator.SetMethodToHuang()
    elif autoThresholdMethod == METHOD_INTERMODES:
      self.autoThresholdCalculator.SetMethodToIntermodes()
    elif autoThresholdMethod == METHOD_ISO_DATA:
      self.autoThresholdCalculator.SetMethodToIsoData()
    elif autoThresholdMethod == METHOD_KITTLER_ILLINGWORTH:
      self.autoThresholdCalculator.SetMethodToKittlerIllingworth()
    elif autoThresholdMethod == METHOD_LI:
      self.autoThresholdCalculator.SetMethodToLi()
    elif autoThresholdMethod == METHOD_MAXIMUM_ENTROPY:
      self.autoThresholdCalculator.SetMethodToMaximumEntropy()
    elif autoThresholdMethod == METHOD_MOMENTS:
      self.autoThresholdCalculator.SetMethodToMoments()
    elif autoThresholdMethod == METHOD_OTSU:
      self.autoThresholdCalculator.SetMethodToOtsu()
    elif autoThresholdMethod == METHOD_RENYI_ENTROPY:
      self.autoThresholdCalculator.SetMethodToRenyiEntropy()
    elif autoThresholdMethod == METHOD_SHANBHAG:
      self.autoThresholdCalculator.SetMethodToShanbhag()
    elif autoThresholdMethod == METHOD_TRIANGLE:
      self.autoThresholdCalculator.SetMethodToTriangle()
    elif autoThresholdMethod == METHOD_YEN:
      self.autoThresholdCalculator.SetMethodToYen()
    else:
      logging.error("Unknown AutoThresholdMethod {0}".format(autoThresholdMethod))

    masterImageData = self.scriptedEffect.masterVolumeImageData()
    self.autoThresholdCalculator.SetInputData(masterImageData)

    self.autoThresholdCalculator.Update()
    computedThreshold = self.autoThresholdCalculator.GetThreshold()

    masterVolumeMin, masterVolumeMax = masterImageData.GetScalarRange()

    if autoThresholdMode == MODE_SET_UPPER:
      self.scriptedEffect.setParameter("MaximumThreshold", computedThreshold)
    elif autoThresholdMode == MODE_SET_LOWER:
      self.scriptedEffect.setParameter("MinimumThreshold", computedThreshold)
    elif autoThresholdMode == MODE_SET_MIN_UPPER:
      self.scriptedEffect.setParameter("MinimumThreshold", masterVolumeMin)
      self.scriptedEffect.setParameter("MaximumThreshold", computedThreshold)
    elif autoThresholdMode == MODE_SET_LOWER_MAX:
      self.scriptedEffect.setParameter("MinimumThreshold", computedThreshold)
      self.scriptedEffect.setParameter("MaximumThreshold", masterVolumeMax)
    else:
      logging.error("Unknown AutoThresholdMode {0}".format(autoThresholdMode))

  def onApply(self):
    try:
      # Get master volume image data
      import vtkSegmentationCorePython as vtkSegmentationCore
      masterImageData = self.scriptedEffect.masterVolumeImageData()
      # Get modifier labelmap
      modifierLabelmap = self.scriptedEffect.defaultModifierLabelmap()
      originalImageToWorldMatrix = vtk.vtkMatrix4x4()
      modifierLabelmap.GetImageToWorldMatrix(originalImageToWorldMatrix)
      # Get parameters
      min = self.scriptedEffect.doubleParameter("MinimumThreshold")
      max = self.scriptedEffect.doubleParameter("MaximumThreshold")

      self.scriptedEffect.saveStateForUndo()

      # Perform thresholding
      thresh = vtk.vtkImageThreshold()
      thresh.SetInputData(masterImageData)
      thresh.ThresholdBetween(min, max)
      thresh.SetInValue(1)
      thresh.SetOutValue(0)
      thresh.SetOutputScalarType(modifierLabelmap.GetScalarType())
      thresh.Update()
      modifierLabelmap.DeepCopy(thresh.GetOutput())
    except IndexError:
      logging.error('apply: Failed to threshold master volume!')
      pass

    # Apply changes
    self.scriptedEffect.modifySelectedSegmentByLabelmap(modifierLabelmap, slicer.qSlicerSegmentEditorAbstractEffect.ModificationModeSet)

    # De-select effect
    self.scriptedEffect.selectEffect("")

  def clearPreviewDisplay(self):
    for sliceWidget, pipeline in self.previewPipelines.items():
      self.scriptedEffect.removeActor2D(sliceWidget, pipeline.actor)
    self.previewPipelines = {}

  def clearHistogramDisplay(self):
    if self.histogramPipeline is None:
      return
    sliceWidget = self.histogramPipeline.sliceWidget
    self.scriptedEffect.removeActor2D(sliceWidget, self.histogramPipeline.actor)
    self.histogramPipeline = None

  def setupPreviewDisplay(self):
    # Clear previous pipelines before setting up the new ones
    self.clearPreviewDisplay()

    layoutManager = slicer.app.layoutManager()
    if layoutManager is None:
      return

    # Add a pipeline for each 2D slice view
    for sliceViewName in layoutManager.sliceViewNames():
      sliceWidget = layoutManager.sliceWidget(sliceViewName)
      if not self.scriptedEffect.segmentationDisplayableInView(sliceWidget.mrmlSliceNode()):
        continue
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
    if not segmentationNode:
      # scene was closed while preview was active
      return
    displayNode = segmentationNode.GetDisplayNode()
    if displayNode is None:
      logging.error("preview: Invalid segmentation display node!")
      color = [0.5,0.5,0.5]
    segmentID = self.scriptedEffect.parameterSetNode().GetSelectedSegmentID()
    if segmentID is None:
      return

    # Make sure we keep the currently selected segment hidden (the user may have changed selection)
    if segmentID != self.previewedSegmentID:
      self.setCurrentSegmentTransparent()

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

  def processInteractionEvents(self, callerInteractor, eventId, viewWidget):
    abortEvent = False

    masterImageData = self.scriptedEffect.masterVolumeImageData()
    if masterImageData is None:
      return abortEvent

    # Only allow for slice views
    if viewWidget.className() != "qMRMLSliceWidget":
      return abortEvent

    # Clicking in a view should remove all previous pipelines
    if eventId == vtk.vtkCommand.LeftButtonPressEvent:
      self.clearHistogramDisplay()

    if self.histogramPipeline is None:
      self.createHistogramPipeline(viewWidget)

    xy = callerInteractor.GetEventPosition()
    ras = self.xyToRas(xy, viewWidget)

    if eventId == vtk.vtkCommand.LeftButtonPressEvent:
      self.histogramPipeline.state = HISTOGRAM_STATE_MOVING
      self.histogramPipeline.setPoint1(ras)
      self.histogramPipeline.setPoint2(ras)
      self.updateHistogram()
    elif eventId == vtk.vtkCommand.LeftButtonReleaseEvent:
      self.histogramPipeline.state = HISTOGRAM_STATE_PLACED
    elif eventId == vtk.vtkCommand.MouseMoveEvent:
      if self.histogramPipeline.state == HISTOGRAM_STATE_MOVING:
        self.histogramPipeline.setPoint2(ras)
        self.updateHistogram()
    else:
      pass
    return abortEvent

  def createHistogramPipeline(self, sliceWidget):
    brushType = HISTOGRAM_TYPE_CIRCLE
    if self.boxROIButton.checked:
      brushType = HISTOGRAM_TYPE_BOX
    pipeline = HistogramPipeline(self, self.scriptedEffect, sliceWidget, brushType)
    # Add actor
    renderer = self.scriptedEffect.renderer(sliceWidget)
    if renderer is None:
      logging.error("pipelineForWidget: Failed to get renderer!")
      return None
    self.scriptedEffect.addActor2D(sliceWidget, pipeline.actor)
    self.histogramPipeline = pipeline

  def processViewNodeEvents(self, callerViewNode, eventId, viewWidget):
    if self.histogramPipeline is not None:
      self.histogramPipeline.updateBrushModel()

  def setBrushToCircle(self):
    self.brushMode = HISTOGRAM_TYPE_CIRCLE
    self.clearHistogramDisplay()

  def setBrushToBox(self):
    self.brushMode = HISTOGRAM_TYPE_BOX
    self.clearHistogramDisplay()

  def setHistogramToUpper(self):
    if self.histogramPipeline is None:
      return
    self.updateHistogram()

  def setHistogramToRange(self):
    if self.histogramPipeline is None:
      return
    self.updateHistogram()

  def setHistogramToLower(self):
    if self.histogramPipeline is None:
      return
    self.updateHistogram()

  def onHistogramMouseClick(self, pos, button):
    self.histogramStartPosition = pos
    self.histogramEndPosition = pos
    if (button == qt.Qt.RightButton):
      self.selectionFunction.RemoveAllPoints()
      self.averageFunction.RemoveAllPoints()

  def onHistogramMouseMove(self, pos, button):
    self.histogramEndPosition = pos
    if (button == qt.Qt.RightButton):
      return
    self.updateHistogramMouseThreshold()

  def onHistogramMouseRelease(self, pos, button):
    self.histogramEndPosition = pos
    if (button == qt.Qt.RightButton):
      return
    self.updateHistogramMouseThreshold()

  def updateHistogramMouseThreshold(self): # TODO: name
    masterImageData = self.scriptedEffect.masterVolumeImageData()
    if masterImageData is None:
      return
    scalarRange = masterImageData.GetScalarRange()

    startX = min(scalarRange[1], max(scalarRange[0], self.histogramStartPosition.x()))
    endX = min(scalarRange[1], max(scalarRange[0], self.histogramEndPosition.x()))

    minimum = min(startX, endX)
    average = (startX + endX) / 2.0
    maximum = max(startX, endX)

    lowerThreshold = minimum
    upperThreshold = maximum

    if self.lowerHistogramButton.checked:
      lowerThreshold = minimum
      upperThreshold = average
    elif self.upperHistogramButton.checked:
      lowerThreshold = average
      upperThreshold = maximum

    self.scriptedEffect.setParameter("MinimumThreshold", lowerThreshold)
    self.scriptedEffect.setParameter("MaximumThreshold", upperThreshold)

    epsilon = 0.00001
    self.selectionFunction.RemoveAllPoints()
    self.selectionFunction.AddPoint(minimum - epsilon, 0.0)
    self.selectionFunction.AddPoint(minimum, 1.0)
    self.selectionFunction.AddPoint(minimum + epsilon, 0.0)
    self.selectionFunction.AddPoint(maximum - epsilon, 0.0)
    self.selectionFunction.AddPoint(maximum, 1.0)
    self.selectionFunction.AddPoint(maximum + epsilon, 0.0)
    self.selectionFunction.AdjustRange(scalarRange)

    self.averageFunction.RemoveAllPoints()
    self.averageFunction.AddPoint(average - epsilon, 0.0)
    self.averageFunction.AddPoint(average, 1.0)
    self.averageFunction.AddPoint(average + epsilon, 0.0)
    self.averageFunction.AdjustRange(scalarRange)


  def updateHistogram(self):
    masterImageData = self.scriptedEffect.masterVolumeImageData()
    if masterImageData is None or self.histogramPipeline is None:
      self.histogramFunction.RemoveAllPoints()
      return

    # Ensure that the brush is in the correct location
    self.histogramPipeline.updateBrushModel()

    self.stencil.SetInputConnection(self.histogramPipeline.worldToSliceTransformer.GetOutputPort())

    self.histogramPipeline.worldToSliceTransformer.Update()
    brushPolydata = self.histogramPipeline.worldToSliceTransformer.GetOutput()
    brushBounds = brushPolydata.GetBounds()
    brushExtent = [0, -1, 0, -1, 0, -1]
    for i in range(3):
      brushExtent[2*i] = vtk.vtkMath.Floor(brushBounds[2*i])
      brushExtent[2*i+1] = vtk.vtkMath.Ceil(brushBounds[2*i+1])
    if brushExtent[0] > brushExtent[1] or brushExtent[2] > brushExtent[3] or brushExtent[4] > brushExtent[5]:
      self.histogramFunction.RemoveAllPoints()
      return

    sliceLogic = self.histogramPipeline.sliceWidget.sliceLogic()
    backgroundLogic = sliceLogic.GetBackgroundLayer()
    self.reslice.SetInputConnection(backgroundLogic.GetReslice().GetInputConnection(0, 0))
    self.reslice.SetResliceTransform(backgroundLogic.GetReslice().GetResliceTransform())
    self.reslice.SetInterpolationMode(backgroundLogic.GetReslice().GetInterpolationMode())
    self.reslice.SetOutputExtent(brushExtent)

    maxNumberOfBins = 1000
    masterImageData = self.scriptedEffect.masterVolumeImageData()
    scalarRange = masterImageData.GetScalarRange()
    numberOfBins = int(scalarRange[1] - scalarRange[0]) + 1
    if numberOfBins > maxNumberOfBins:
      numberOfBins = maxNumberOfBins
    binSpacing = (scalarRange[1] - scalarRange[0] + 1) / numberOfBins

    self.imageAccumulate.SetComponentExtent(0, numberOfBins - 1, 0, 0, 0, 0)
    self.imageAccumulate.SetComponentSpacing(binSpacing, binSpacing, binSpacing)
    self.imageAccumulate.SetComponentOrigin(scalarRange[0], scalarRange[0], scalarRange[0])

    self.imageAccumulate.Update()

    self.histogramFunction.RemoveAllPoints()
    tableSize = self.imageAccumulate.GetOutput().GetPointData().GetScalars().GetNumberOfTuples()
    for i in range(tableSize):
      value = self.imageAccumulate.GetOutput().GetPointData().GetScalars().GetTuple1(i)
      self.histogramFunction.AddPoint(i + scalarRange[0], value)
    self.histogramFunction.AdjustRange(scalarRange)


    if self.upperHistogramButton.checked:
      self.scriptedEffect.setParameter("MinimumThreshold", self.imageAccumulate.GetMean()[0])
      self.scriptedEffect.setParameter("MaximumThreshold", self.imageAccumulate.GetMax()[0])
    elif self.lowerHistogramButton.checked:
      self.scriptedEffect.setParameter("MinimumThreshold", self.imageAccumulate.GetMin()[0])
      self.scriptedEffect.setParameter("MaximumThreshold", self.imageAccumulate.GetMean()[0])
    elif self.rangeHistogramButton.checked:
      self.scriptedEffect.setParameter("MinimumThreshold", self.imageAccumulate.GetMin()[0])
      self.scriptedEffect.setParameter("MaximumThreshold", self.imageAccumulate.GetMax()[0])

#
# PreviewPipeline
#
class PreviewPipeline(object):
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

###

#
# Histogram threshold
#
class HistogramPipeline(object):

  def __init__(self, thresholdEffect, scriptedEffect, sliceWidget, brushMode):
    self.thresholdEffect = thresholdEffect
    self.scriptedEffect = scriptedEffect
    self.sliceWidget = sliceWidget
    self.brushMode = brushMode
    self.state = HISTOGRAM_STATE_OFF

    self.point1 = [0,0,0]
    self.point2 = [0,0,0]

    # Actor setup
    self.brushCylinderSource = vtk.vtkCylinderSource()
    self.brushCylinderSource.SetResolution(32)

    self.brushCubeSource = vtk.vtkCubeSource()

    self.brushToWorldOriginTransform = vtk.vtkTransform()
    self.brushToWorldOriginTransformer = vtk.vtkTransformPolyDataFilter()
    self.brushToWorldOriginTransformer.SetTransform(self.brushToWorldOriginTransform)
    self.brushToWorldOriginTransformer.SetInputConnection(self.brushCylinderSource.GetOutputPort())

    self.normalFilter = vtk.vtkPolyDataNormals()
    self.normalFilter.AutoOrientNormalsOn()
    self.normalFilter.SetInputConnection(self.brushToWorldOriginTransformer.GetOutputPort())

    # Brush to RAS transform
    self.worldOriginToWorldTransform = vtk.vtkTransform()
    self.worldOriginToWorldTransformer = vtk.vtkTransformPolyDataFilter ()
    self.worldOriginToWorldTransformer.SetTransform(self.worldOriginToWorldTransform)
    self.worldOriginToWorldTransformer.SetInputConnection(self.normalFilter.GetOutputPort())

    # RAS to XY transform
    self.worldToSliceTransform = vtk.vtkTransform()
    self.worldToSliceTransformer = vtk.vtkTransformPolyDataFilter ()
    self.worldToSliceTransformer.SetTransform(self.worldToSliceTransform)
    self.worldToSliceTransformer.SetInputConnection(self.worldOriginToWorldTransformer.GetOutputPort())

    # Cutting takes place in XY coordinates
    self.slicePlane = vtk.vtkPlane()
    self.slicePlane.SetNormal(0, 0, 1)
    self.slicePlane.SetOrigin(0, 0, 0)
    self.cutter = vtk.vtkCutter()
    self.cutter.SetCutFunction(self.slicePlane)
    self.cutter.SetInputConnection(self.worldToSliceTransformer.GetOutputPort())

    self.mapper = vtk.vtkPolyDataMapper2D()
    self.mapper.SetInputConnection(self.cutter.GetOutputPort())

    self.actor = vtk.vtkActor2D()
    self.actor.SetMapper(self.mapper)
    actorProperty = self.actor.GetProperty()
    actorProperty.SetColor(1,1,0)
    actorProperty.SetLineWidth(1)

  def setPoint1(self, ras):
    self.point1 = ras
    self.updateBrushModel()

  def setPoint2(self, ras):
    self.point2 = ras
    self.updateBrushModel()

  def updateBrushModel(self):

    # Update slice cutting plane position and orientation
    sliceXyToRas = self.sliceWidget.sliceLogic().GetSliceNode().GetXYToRAS()
    rasToSliceXy = vtk.vtkMatrix4x4()
    vtk.vtkMatrix4x4.Invert(sliceXyToRas, rasToSliceXy)
    self.worldToSliceTransform.SetMatrix(rasToSliceXy)

    # brush is rotated to the slice widget plane
    brushToWorldOriginTransformMatrix = vtk.vtkMatrix4x4()
    brushToWorldOriginTransformMatrix.DeepCopy(self.sliceWidget.sliceLogic().GetSliceNode().GetSliceToRAS())
    brushToWorldOriginTransformMatrix.SetElement(0,3, 0)
    brushToWorldOriginTransformMatrix.SetElement(1,3, 0)
    brushToWorldOriginTransformMatrix.SetElement(2,3, 0)

    self.brushToWorldOriginTransform.Identity()
    self.brushToWorldOriginTransform.Concatenate(brushToWorldOriginTransformMatrix)
    self.brushToWorldOriginTransform.RotateX(90) # cylinder's long axis is the Y axis, we need to rotate it to Z axis

    sliceSpacingMm = self.scriptedEffect.sliceSpacing(self.sliceWidget)

    center = [0,0,0]
    if self.brushMode == HISTOGRAM_TYPE_CIRCLE:
      center = self.point1

      point1ToPoint2 = [0,0,0]
      vtk.vtkMath.Subtract(self.point1, self.point2, point1ToPoint2)
      radius = vtk.vtkMath.Normalize(point1ToPoint2)

      self.brushToWorldOriginTransformer.SetInputConnection(self.brushCylinderSource.GetOutputPort())
      self.brushCylinderSource.SetRadius(radius)
      self.brushCylinderSource.SetHeight(sliceSpacingMm)

    elif self.brushMode == HISTOGRAM_TYPE_BOX:
      self.brushToWorldOriginTransformer.SetInputConnection(self.brushCubeSource.GetOutputPort())

      length = [0,0,0]
      for i in range(3):
        center[i] = (self.point1[i] + self.point2[i]) / 2.0
        length[i] = abs(self.point1[i] - self.point2[i])

      xVector = [1,0,0,0]
      self.brushToWorldOriginTransform.MultiplyPoint(xVector, xVector)
      xLength = abs(vtk.vtkMath.Dot(xVector[:3], length))
      self.brushCubeSource.SetXLength(xLength)

      zVector = [0,0,1,0]
      self.brushToWorldOriginTransform.MultiplyPoint(zVector, zVector)
      zLength = abs(vtk.vtkMath.Dot(zVector[:3], length))
      self.brushCubeSource.SetZLength(zLength)

      self.brushCubeSource.SetYLength(sliceSpacingMm)

    self.worldOriginToWorldTransform.Identity()
    self.worldOriginToWorldTransform.Translate(center)

    self.sliceWidget.sliceView().scheduleRender()

HISTOGRAM_TYPE_CIRCLE = 'CIRCLE'
HISTOGRAM_TYPE_BOX = 'BOX'
HISTOGRAM_TYPE_LINE = 'LINE'

HISTOGRAM_STATE_OFF = 'OFF'
HISTOGRAM_STATE_MOVING = 'MOVING'
HISTOGRAM_STATE_PLACED = 'PLACED'
HISTOGRAM_STATE_SELECTED = 'SELECTION'
###

METHOD_HUANG = 'HUANG'
METHOD_INTERMODES = 'INTERMODES'
METHOD_ISO_DATA = 'ISO_DATA'
METHOD_KITTLER_ILLINGWORTH = 'KITTLER_ILLINGWORTH'
METHOD_LI = 'LI'
METHOD_MAXIMUM_ENTROPY = 'MAXIMUM_ENTROPY'
METHOD_MOMENTS = 'MOMENTS'
METHOD_OTSU = 'OTSU'
METHOD_RENYI_ENTROPY = 'RENYI_ENTROPY'
METHOD_SHANBHAG = 'SHANBHAG'
METHOD_TRIANGLE = 'TRIANGLE'
METHOD_YEN = 'YEN'

MODE_SET_UPPER = 'SET_UPPER'
MODE_SET_LOWER = 'SET_LOWER'
MODE_SET_RANGE = 'SET_RANGE'
MODE_SET_MIN_UPPER = 'SET_MIN_UPPER'
MODE_SET_LOWER_MAX = 'SET_LOWER_MAX'
