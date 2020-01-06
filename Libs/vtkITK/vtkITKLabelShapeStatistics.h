/*=========================================================================

  Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==========================================================================*/

#ifndef __vtkITKLabelShapeStatistics_h
#define __vtkITKLabelShapeStatistics_h

#include "vtkITK.h"
#include "vtkSimpleImageToImageFilter.h"

// VTK includes
#include <vtkMatrix4x4.h>
#include <vtkSmartPointer.h>
#include <vtkVector.h>

// std includes
#include <map>

class vtkPoints;

/// \brief ITK-based utilities for calculating label statistics.
class VTK_ITK_EXPORT vtkITKLabelShapeStatistics : public vtkSimpleImageToImageFilter
{
 public:
  static vtkITKLabelShapeStatistics *New();
  vtkTypeMacro(vtkITKLabelShapeStatistics, vtkSimpleImageToImageFilter);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  vtkGetMacro(ComputeFeretDiameter, bool);
  vtkSetMacro(ComputeFeretDiameter, bool);
  vtkBooleanMacro(ComputeFeretDiameter, bool);

  vtkGetMacro(ComputeOrientedBoundingBox, bool);
  vtkSetMacro(ComputeOrientedBoundingBox, bool);
  vtkBooleanMacro(ComputeOrientedBoundingBox, bool);

  vtkGetMacro(ComputePerimeter, bool);
  vtkSetMacro(ComputePerimeter, bool);
  vtkBooleanMacro(ComputePerimeter, bool);

  void ClearCentroids();
  void AddCentroid(int value, double centroid[3]);
  bool GetCentroid(int value, double* centroid);

  void ClearOrientedBoundingBox();
  void AddBoundingBox(int label, vtkMatrix4x4* directions, vtkVector3d origin, vtkVector3d size, vtkPoints* points);
  void GetOrientedBoundingBoxDirection(int value, vtkMatrix4x4* direction);
  void GetOrientedBoundingBoxOrigin(int value, double* origin);
  void GetOrientedBoundingBoxSize(int value, double* origin);
  //void GetOrientedBoundingBoxVertices(int value, double* origin);

  vtkSetObjectMacro(Directions, vtkMatrix4x4);

protected:
  vtkITKLabelShapeStatistics();
  ~vtkITKLabelShapeStatistics() override;

  void SimpleExecute(vtkImageData* input, vtkImageData* output) override;

  std::map<int, vtkVector3d> Centroids;

  std::map<int, vtkSmartPointer<vtkMatrix4x4> > OrientedBoundingBoxDirection;
  std::map<int, vtkVector3d> OrientedBoundingBoxOrigin;
  std::map<int, vtkVector3d> OrientedBoundingBoxSize;
  std::map<int, vtkSmartPointer<vtkPoints> > OrientedBoundingBoxVertices;

  bool ComputeFeretDiameter;
  bool ComputeOrientedBoundingBox;
  bool ComputePerimeter;

  vtkMatrix4x4* Directions;

private:
  vtkITKLabelShapeStatistics(const vtkITKLabelShapeStatistics&) = delete;
  void operator=(const vtkITKLabelShapeStatistics&) = delete;
};

#endif
