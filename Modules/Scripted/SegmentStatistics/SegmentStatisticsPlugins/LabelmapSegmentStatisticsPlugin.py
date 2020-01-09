import vtk
import slicer
import vtkITK
from SegmentStatisticsPlugins import SegmentStatisticsPluginBase
from functools import reduce

class LabelmapSegmentStatisticsPlugin(SegmentStatisticsPluginBase):
  """Statistical plugin for Labelmaps"""

  def __init__(self):
    super(LabelmapSegmentStatisticsPlugin,self).__init__()
    self.name = "Labelmap"
    self.obbKeys = ["obb_origin_ras", "obb_diameter_mm", "obb_direction_ras_x", "obb_direction_ras_y", "obb_direction_ras_z"]
    self.shapeKeys = ["centroid_ras", "feret_diameter_mm", "surface_mm2", "roundness"] + self.obbKeys
    self.keys = ["voxel_count", "volume_mm3", "volume_cm3"] + self.shapeKeys
    self.defaultKeys = self.keys # calculate all measurements by default
    #... developer may add extra options to configure other parameters

  def computeStatistics(self, segmentID):
    import vtkSegmentationCorePython as vtkSegmentationCore
    requestedKeys = self.getRequestedKeys()

    segmentationNode = slicer.mrmlScene.GetNodeByID(self.getParameterNode().GetParameter("Segmentation"))

    if len(requestedKeys)==0:
      return {}

    containsLabelmapRepresentation = segmentationNode.GetSegmentation().ContainsRepresentation(
      vtkSegmentationCore.vtkSegmentationConverter.GetSegmentationBinaryLabelmapRepresentationName())
    if not containsLabelmapRepresentation:
      return {}

    segmentLabelmap = slicer.vtkOrientedImageData()
    segmentationNode.GetBinaryLabelmapRepresentation(segmentID, segmentLabelmap)
    if (not segmentLabelmap
      or not segmentLabelmap.GetPointData()
      or not segmentLabelmap.GetPointData().GetScalars()):
      # No input label data
      return {}

    # We need to know exactly the value of the segment voxels, apply threshold to make force the selected label value
    labelValue = 1
    backgroundValue = 0
    thresh = vtk.vtkImageThreshold()
    thresh.SetInputData(segmentLabelmap)
    thresh.ThresholdByLower(0)
    thresh.SetInValue(backgroundValue)
    thresh.SetOutValue(labelValue)
    thresh.SetOutputScalarType(vtk.VTK_UNSIGNED_CHAR)
    thresh.Update()

    #  Use binary labelmap as a stencil
    stencil = vtk.vtkImageToImageStencil()
    stencil.SetInputData(thresh.GetOutput())
    stencil.ThresholdByUpper(labelValue)
    stencil.Update()

    stat = vtk.vtkImageAccumulate()
    stat.SetInputData(thresh.GetOutput())
    stat.SetStencilData(stencil.GetOutput())
    stat.Update()

    # Add data to statistics list
    cubicMMPerVoxel = reduce(lambda x,y: x*y, segmentLabelmap.GetSpacing())
    ccPerCubicMM = 0.001
    stats = {}
    if "voxel_count" in requestedKeys:
      stats["voxel_count"] = stat.GetVoxelCount()
    if "volume_mm3" in requestedKeys:
      stats["volume_mm3"] = stat.GetVoxelCount() * cubicMMPerVoxel
    if "volume_cm3" in requestedKeys:
      stats["volume_cm3"] = stat.GetVoxelCount() * cubicMMPerVoxel * ccPerCubicMM

    calculateShapeStats = False
    for shapeKey in self.shapeKeys:
      if shapeKey in requestedKeys:
        calculateShapeStats = True
        break

    if calculateShapeStats:
      calculateOBB = False
      for obbKey in self.obbKeys:
        if obbKey in requestedKeys:
          calculateOBB = True
          break

      directions = vtk.vtkMatrix4x4()
      segmentLabelmap.GetDirectionMatrix(directions)

      shapeStat = vtkITK.vtkITKLabelShapeStatistics()
      shapeStat.SetInputData(thresh.GetOutput())
      shapeStat.SetDirections(directions)
      if calculateOBB:
        shapeStat.ComputeOrientedBoundingBoxOn()
      if "feret_diameter_mm" in requestedKeys:
        shapeStat.ComputeFeretDiameterOn()
      if "surface_mm2" in requestedKeys:
        shapeStat.ComputePerimeterOn()
      shapeStat.Update()

      # If segmentation node is transformed, apply that transform to get RAS coordinates
      transformSegmentToRas = vtk.vtkGeneralTransform()
      slicer.vtkMRMLTransformNode.GetTransformBetweenNodes(segmentationNode.GetParentTransformNode(), None, transformSegmentToRas)

      if "centroid_ras" in requestedKeys:
        centroid = [0,0,0]
        centroidRAS = [0,0,0]
        shapeStat.GetCentroid(labelValue, centroid)
        transformSegmentToRas.TransformPoint(centroid, centroidRAS)
        stats["centroid_ras"] = centroidRAS
      if "roundness" in requestedKeys:
        roundness = shapeStat.GetRoundness(labelValue)
        stats["roundness"] = roundness
      if "feret_diameter_mm" in requestedKeys:
        feretDiameter = shapeStat.GetFeretDiameter(labelValue)
        stats["feret_diameter_mm"] = feretDiameter
      if "surface_mm2" in requestedKeys:
        perimeter = shapeStat.GetPerimeter(labelValue)
        stats["surface_mm2"] = perimeter
      if "obb_origin_ras" in requestedKeys:
        obbOrigin = [0,0,0]
        obbOriginRAS = [0,0,0]
        shapeStat.GetOrientedBoundingBoxOrigin(labelValue, obbOrigin)
        transformSegmentToRas.TransformPoint(obbOrigin, obbOriginRAS)
        stats["obb_origin_ras"] = obbOriginRAS
      if "obb_diameter_mm" in requestedKeys:
        obbDiameterMM = [0,0,0]
        shapeStat.GetOrientedBoundingBoxSize(labelValue, obbDiameterMM)
        stats["obb_diameter_mm"] = obbDiameterMM
      if "obb_direction_ras_x" in requestedKeys:
        obbOrigin = [0,0,0]
        shapeStat.GetOrientedBoundingBoxOrigin(labelValue, obbOrigin)

        directions = vtk.vtkMatrix4x4()
        shapeStat.GetOrientedBoundingBoxDirection(labelValue, directions)

        obbDirectionX = [0,0,0]
        obbDirectionY = [0,0,0]
        obbDirectionZ = [0,0,0]
        for i in range(3):
          obbDirectionX[i] = directions.GetElement(0, i)
          obbDirectionY[i] = directions.GetElement(1, i)
          obbDirectionZ[i] = directions.GetElement(2, i)
        transformSegmentToRas.TransformVectorAtPoint(obbOrigin, obbDirectionX, obbDirectionX)
        transformSegmentToRas.TransformVectorAtPoint(obbOrigin, obbDirectionY, obbDirectionY)
        transformSegmentToRas.TransformVectorAtPoint(obbOrigin, obbDirectionZ, obbDirectionZ)
        stats["obb_direction_ras_x"] = obbDirectionX
        stats["obb_direction_ras_y"] = obbDirectionY
        stats["obb_direction_ras_z"] = obbDirectionZ

    return stats

  def getMeasurementInfo(self, key):
    """Get information (name, description, units, ...) about the measurement for the given key"""
    info = {}

    # @fedorov could not find any suitable DICOM quantity code for "number of voxels".
    # DCM has "Number of needles" etc., so probably "Number of voxels"
    # should be added too. Need to discuss with @dclunie. For now, a
    # QIICR private scheme placeholder.
    info["voxel_count"] = \
      self.createMeasurementInfo(name="Voxel count", description="Number of voxels", units="voxels",
                                   quantityDicomCode=self.createCodedEntry("nvoxels", "99QIICR", "Number of voxels", True),
                                   unitsDicomCode=self.createCodedEntry("voxels", "UCUM", "voxels", True))

    info["volume_mm3"] = \
      self.createMeasurementInfo(name="Volume mm3", description="Volume in mm3", units="mm3",
                                   quantityDicomCode=self.createCodedEntry("G-D705", "SRT", "Volume", True),
                                   unitsDicomCode=self.createCodedEntry("mm3", "UCUM", "cubic millimeter", True))

    info["volume_cm3"] = \
      self.createMeasurementInfo(name="Volume cm3", description="Volume in cm3", units="cm3",
                                   quantityDicomCode=self.createCodedEntry("G-D705", "SRT", "Volume", True),
                                   unitsDicomCode=self.createCodedEntry("cm3", "UCUM", "cubic centimeter", True),
                                   measurementMethodDicomCode=self.createCodedEntry("126030", "DCM",
                                                                             "Sum of segmented voxel volumes", True))

    info["centroid_ras"] = \
      self.createMeasurementInfo(name="Centroid (RAS)", description="Location of the centroid in RAS", units="")

    info["feret_diameter_mm"] = \
      self.createMeasurementInfo(name="Feret Diameter mm", description="Feret diameter in mm", units="mm")

    info["surface_mm2"] = \
      self.createMeasurementInfo(name="Surface mm2", description="Surface area in mm2", units="mm2",
                                   quantityDicomCode=self.createCodedEntry("000247", "99CHEMINF", "Surface area", True),
                                   unitsDicomCode=self.createCodedEntry("mm2", "UCUM", "squared millimeters", True))

    info["roundness"] = \
      self.createMeasurementInfo(name="Roundness", description="Roundness", units="")

    info["obb_origin_ras"] = \
      self.createMeasurementInfo(name="OBB Origin (RAS)", description="OBB origin", units="")

    info["obb_diameter_mm"] = \
      self.createMeasurementInfo(name="OBB diameter", description="OBB diameter", units="mm")

    info["obb_direction_ras_x"] = \
      self.createMeasurementInfo(name="OBB X direction (RAS)", description="OBB X direction", units="")

    info["obb_direction_ras_y"] = \
      self.createMeasurementInfo(name="OBB Y direction (RAS)", description="OBB Y direction", units="")

    info["obb_direction_ras_z"] = \
      self.createMeasurementInfo(name="OBB Z direction (RAS)", description="OBB Z direction", units="")

    return info[key] if key in info else None
