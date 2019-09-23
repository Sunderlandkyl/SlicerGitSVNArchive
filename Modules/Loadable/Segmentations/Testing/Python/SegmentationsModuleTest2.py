import os
import unittest
import vtk, qt, ctk, slicer
import logging
from slicer.ScriptedLoadableModule import *

import vtkSegmentationCore


class SegmentationsModuleTest2(unittest.TestCase):

  def setUp(self):
    """ Do whatever is needed to reset the state - typically a scene clear will be enough.
    """
    slicer.mrmlScene.Clear(0)

  def runTest(self):
    """Run as few or as many tests as needed here.
    """
    self.setUp()
    self.test_SegmentationsModuleTest2()

  #------------------------------------------------------------------------------
  def test_SegmentationsModuleTest2(self):
    # Check for modules
    self.assertIsNotNone( slicer.modules.segmentations )

    self.TestSection_TestMergedLabelmapCasting()

    logging.info('Test finished')

  #------------------------------------------------------------------------------
  def TestSection_TestMergedLabelmapCasting(self):

    labelmap = slicer.vtkOrientedImageData()
    labelmap.SetDimensions(1,1,1)
    labelmap.AllocateScalars(vtk.VTK_UNSIGNED_CHAR, 1)

    segment = slicer.vtkSegment()
    segment.AddRepresentation(vtkSegmentationCore.vtkSegmentationConverter.GetSegmentationBinaryLabelmapRepresentationName(), labelmap)

    segmentationNode = slicer.mrmlScene.AddNewNodeByClass('vtkMRMLSegmentationNode')
    segmentation = segmentationNode.GetSegmentation()
    segmentation.AddSegment(segment, "Segment_1")

    self.assertEqual(labelmap.GetScalarType(), vtk.VTK_UNSIGNED_CHAR) # Inital labelmap type should be unsigned char
    for i in range(254):
      segmentation.AddEmptySegment()
    self.assertEqual(labelmap.GetScalarType(), vtk.VTK_UNSIGNED_CHAR) # 255 labelmaps can still fit in an unsigned char
    segmentation.AddEmptySegment() # 256 labelmaps cannot fit in an unsigned char
    self.assertEqual(labelmap.GetScalarType(), vtk.VTK_UNSIGNED_SHORT) # Labelmap should have been cast to an unsigned short
