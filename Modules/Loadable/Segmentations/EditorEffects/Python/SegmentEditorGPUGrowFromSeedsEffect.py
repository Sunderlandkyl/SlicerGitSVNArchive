import os
import vtk, qt, ctk, slicer
import logging
from SegmentEditorEffects import *

class SegmentEditorGPUGrowFromSeedsEffect(AbstractScriptedSegmentEditorAutoCompleteEffect):
  """ AutoCompleteEffect is an effect that can create a full segmentation
      from a partial segmentation (not all slices are segmented or only
      part of the target structures are painted).
  """

  def __init__(self, scriptedEffect):
    AbstractScriptedSegmentEditorAutoCompleteEffect.__init__(self, scriptedEffect)
    scriptedEffect.name = 'GPU Grow from seeds'
    self.minimumNumberOfSegments = 2
    self.clippedMasterImageDataRequired = True # master volume intensities are used by this effect
    self.clippedMaskImageDataRequired = True # masking is used
    self.growCutFilter = None

  def clone(self):
    import qSlicerSegmentationsEditorEffectsPythonQt as effects
    clonedEffect = effects.qSlicerSegmentEditorScriptedEffect(None)
    clonedEffect.setPythonSource(__file__.replace('\\','/'))
    return clonedEffect

  def icon(self):
    iconPath = os.path.join(os.path.dirname(__file__), 'Resources/Icons/GrowFromSeeds.png')
    if os.path.exists(iconPath):
      return qt.QIcon(iconPath)
    return qt.QIcon()

  def helpText(self):
    return """<html>Growing segments to create complete segmentation<br>.
Location, size, and shape of initial segments and content of master volume are taken into account.
Final segment boundaries will be placed where master volume brightness changes abruptly. Instructions:<p>
<ul style="margin: 0">
<li>Use Paint or other offects to draw seeds in each region that should belong to a separate segment.
Paint each seed with a different segment. Minimum two segments are required.</li>
<li>Click <dfn>Initialize</dfn> to compute preview of full segmentation.</li>
<li>Browse through image slices. If previewed segmentation result is not correct then switch to
Paint or other effects and add more seeds in the misclassified region. Full segmentation will be
updated automatically within a few seconds</li>
<li>Click <dfn>Apply</dfn> to update segmentation with the previewed result.</li>
</ul><p>
If segments overlap, segment higher in the segments table will have priority.
The effect uses <a href="https://www.spl.harvard.edu/publications/item/view/2761">fast grow-cut method</a>.
<p></html>"""


  def reset(self):
    self.growCutFilter = None
    self.imageToGPUConverter = None
    self.imageToGPUConverter2 = None
    self.imageToGPUConverter3 = None
    self.gpuToImageConverter = None

    AbstractScriptedSegmentEditorAutoCompleteEffect.reset(self)
    self.updateGUIFromMRML()

  def computePreviewLabelmap(self, mergedImage, outputLabelmap):
    import vtkSlicerSegmentationsModuleLogicPython as vtkSlicerSegmentationsModuleLogic

    if not self.imageToGPUConverter:
      self.imageToGPUConverter = vtk.vtkImageToGPUImageFilter()

    if not self.imageToGPUConverter2:
      self.imageToGPUConverter2 = vtk.vtkImageToGPUImageFilter()

    if not self.imageToGPUConverter3:
      self.imageToGPUConverter3 = vtk.vtkImageToGPUImageFilter()

    self.imageToGPUConverter.SetInputDataObject(mergedImage)
    self.imageToGPUConverter.Update()
    self.renderWindow = self.imageToGPUConverter.GetRenderWindow()


    strengthImage = vtk.vtkImageData()
    strengthImage.SetExtent(mergedImage.GetExtent())
    strengthImage.AllocateScalars(vtk.VTK_FLOAT, 1)

    self.imageToGPUConverter2.SetRenderWindow(self.renderWindow)
    self.imageToGPUConverter2.SetInputDataObject(strengthImage)
    self.imageToGPUConverter2.Update()

    self.imageToGPUConverter3.SetRenderWindow(self.renderWindow)
    self.imageToGPUConverter3.SetInputDataObject(self.clippedMasterImageData)
    self.imageToGPUConverter3.Update()


    mergedGPUImage = self.imageToGPUConverter.GetOutput()
    strengthGPUImage = self.imageToGPUConverter2.GetOutput()
    masterGPUImage = self.imageToGPUConverter3.GetOutput()

    shaderInputs = [{'Label' : mergedGPUImage, 'Strength' : strengthGPUImage}, {'Label' : None, 'Strength' : None}]

    if not self.growCutFilter:
      self.growCutFilter = slicer.vtkGPUGrowCutFilter()
    self.growCutFilter.SetRenderWindow(self.renderWindow)

    for i in range(50):
      label = shaderInputs[i%2]['Label']
      strength = shaderInputs[i%2]['Strength']

      self.growCutFilter.RemoveAllInputs()
      self.growCutFilter.AddInputData(masterGPUImage)
      self.growCutFilter.AddInputData(label)
      self.growCutFilter.AddInputData(strength)

      shaderInputs[(i+1)%2]['Label'] = self.growCutFilter.GetOutput(0)
      shaderInputs[(i+1)%2]['Strength'] = self.growCutFilter.GetOutput(1)
      print(i)
      print(label)
      print(strength)

    if not self.gpuToImageConverter:
      self.gpuToImageConverter = vtk.vtkGPUImageToImageFilter()
    self.gpuToImageConverter.SetRenderWindow(self.renderWindow)
    self.gpuToImageConverter.SetInputDataObject(shaderInputs[0]['Label'])
    self.gpuToImageConverter.Update()
    outputLabelmap.DeepCopy( self.growCutFilter.GetOutput() )
