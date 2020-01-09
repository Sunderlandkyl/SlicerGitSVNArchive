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
#include <vtkTable.h>
#include <vtkVector.h>

// std includes
#include <set>

class vtkPoints;

/// \brief ITK-based utilities for calculating label statistics.
class VTK_ITK_EXPORT vtkITKLabelShapeStatistics : public vtkSimpleImageToImageFilter
{
public:
  static vtkITKLabelShapeStatistics *New();
  vtkTypeMacro(vtkITKLabelShapeStatistics, vtkSimpleImageToImageFilter);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  enum ShapeStatistic
  {
    Centroid,
    OrientedBoundingBox,
    FeretDiameter,
    Perimeter,
    Roundness,
    Flatness,
  };

  std::string GetShapeStatisticAsString(ShapeStatistic statistic);
  ShapeStatistic GetShapeStatisticFromString(std::string statisticName);

  vtkSetObjectMacro(Directions, vtkMatrix4x4);
  vtkSetObjectMacro(StatisticsTable, vtkTable);
  vtkGetObjectMacro(StatisticsTable, vtkTable);

  vtkGetMacro(ComputedStatistics, std::set<ShapeStatistic>);

protected:
  vtkITKLabelShapeStatistics();
  ~vtkITKLabelShapeStatistics() override;

  void SimpleExecute(vtkImageData* input, vtkImageData* output) override;

  void ComputeShapeStatisticOn(ShapeStatistic statistic);
  void ComputeShapeStatisticOn(std::string statisticName);
  void ComputeShapeStatisticOff(ShapeStatistic statistic);
  void ComputeShapeStatisticOff(std::string statisticName);
  void SetComputeShapeStatistic(ShapeStatistic statistic, bool state);
  void SetComputeShapeStatistic(std::string statisticName, bool state);

protected:
  std::set<ShapeStatistic> ComputedStatistics;
  vtkMatrix4x4* Directions;
  vtkTable* StatisticsTable;

private:
  vtkITKLabelShapeStatistics(const vtkITKLabelShapeStatistics&) = delete;
  void operator=(const vtkITKLabelShapeStatistics&) = delete;
};

#endif
