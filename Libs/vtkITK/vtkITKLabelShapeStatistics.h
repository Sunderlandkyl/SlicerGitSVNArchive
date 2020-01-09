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

  vtkSetObjectMacro(Directions, vtkMatrix4x4);

public:
  void ClearCentroids();
  void AddCentroid(int labelValue, vtkVector3d centroid);
  bool GetCentroid(int labelValue, double* centroid);

  void ClearOrientedBoundingBox();
  void AddBoundingBox(int labelValue, vtkMatrix4x4* directions, vtkVector3d origin, vtkVector3d size, vtkPoints* points);
  void GetOrientedBoundingBoxDirection(int labelValue, vtkMatrix4x4* direction);
  void GetOrientedBoundingBoxOrigin(int labelValue, double* origin);
  void GetOrientedBoundingBoxSize(int labelValue, double* size);
  void GetOrientedBoundingBoxVertices(int labelValue, vtkPoints* points);

  void ClearFeretDiameter();
  void AddFeretDiameter(int labelValue, double feretDiameter);
  double GetFeretDiameter(int labelValue);

  void ClearPerimeter();
  void AddPerimeter(int labelValue, double perimeter);
  double GetPerimeter(int labelValue);

  void ClearRoundness();
  void AddRoundness(int labelValue, double roundness);
  double GetRoundness(int labelValue);

  void ClearFlatness();
  void AddFlatness(int labelValue, double flatness);
  double GetFlatness(int labelValue);

protected:
  vtkITKLabelShapeStatistics();
  ~vtkITKLabelShapeStatistics() override;

  void SimpleExecute(vtkImageData* input, vtkImageData* output) override;

  std::map<int, vtkVector3d> Centroids;
  std::map<int, double> FeretDiameter;
  std::map<int, double> Perimeter;
  std::map<int, double> Roundness;
  std::map<int, double> Flatness;
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
