/*=========================================================================

  Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==========================================================================*/

#ifndef __vtkITKLabelShapeStatistics_h
#define __vtkITKLabelShapeStatistics_h

#include "vtkITK.h"

// VTK includes
#include <vtkMatrix4x4.h>
#include <vtkSmartPointer.h>
#include <vtkTable.h>
#include <vtkTableAlgorithm.h>
#include <vtkVector.h>

// std includes
#include <vector>

class vtkPoints;

/// \brief ITK-based utilities for calculating label statistics.
class VTK_ITK_EXPORT vtkITKLabelShapeStatistics : public vtkTableAlgorithm
{
public:
  static vtkITKLabelShapeStatistics *New();
  vtkTypeMacro(vtkITKLabelShapeStatistics, vtkTableAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  enum ShapeStatistic
  {
    Centroid,
    OrientedBoundingBox,
    FeretDiameter,
    Perimeter,
    Roundness,
    Flatness,
    LastStatistc,
  };

  static std::string GetShapeStatisticAsString(ShapeStatistic statistic);
  static ShapeStatistic GetShapeStatisticFromString(std::string statisticName);

  vtkSetObjectMacro(Directions, vtkMatrix4x4);
  vtkSetMacro(ComputedStatistics, std::vector<std::string>);
  vtkGetMacro(ComputedStatistics, std::vector<std::string>);

  void ComputeShapeStatisticOn(std::string statisticName);
  void ComputeShapeStatisticOff(std::string statisticName);
  void SetComputeShapeStatistic(std::string statisticName, bool state);
  bool GetComputeShapeStatistic(std::string statisticName);

protected:
  vtkITKLabelShapeStatistics();
  ~vtkITKLabelShapeStatistics() override;

  int FillInputPortInformation(int vtkNotUsed(port), vtkInformation* info) override;
  virtual int RequestData(vtkInformation* request,
    vtkInformationVector** inputVector,
    vtkInformationVector* outputVector) override;

protected:
  std::vector<std::string> ComputedStatistics;
  vtkMatrix4x4* Directions;

private:
  vtkITKLabelShapeStatistics(const vtkITKLabelShapeStatistics&) = delete;
  void operator=(const vtkITKLabelShapeStatistics&) = delete;
};

#endif
