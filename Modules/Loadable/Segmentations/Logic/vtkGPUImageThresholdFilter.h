/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkGPUImageThresholdFilter.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkGPUImageThresholdFilter
 * @brief   Help image algorithms use the GPU
 *
 * Designed to make it easier to accelerate an image algorithm on the GPU
*/

#ifndef vtkGPUImageThresholdFilter_h
#define vtkGPUImageThresholdFilter_h

#include "vtkSlicerSegmentationsModuleLogicExport.h" // For export macro
#include "vtkGPUAbstractImageFilter.h"

class VTK_SLICER_SEGMENTATIONS_LOGIC_EXPORT vtkGPUImageThresholdFilter : public vtkGPUAbstractImageFilter
{
public:
  static vtkGPUImageThresholdFilter *New();
  vtkTypeMacro(vtkGPUImageThresholdFilter, vtkGPUAbstractImageFilter);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  vtkGetMacro(MaxThreshold, double);
  vtkSetMacro(MaxThreshold, double);

  vtkGetMacro(MinThreshold, double);
  vtkSetMacro(MinThreshold, double);

  vtkGetMacro(OversamplingFactor, int);
  vtkSetMacro(OversamplingFactor, int);


protected:
  vtkGPUImageThresholdFilter();
  ~vtkGPUImageThresholdFilter() VTK_OVERRIDE;

  void UpdateCustomUniformsFragment() override;

private:
  vtkGPUImageThresholdFilter(const vtkGPUImageThresholdFilter&) = delete;
  void operator=(const vtkGPUImageThresholdFilter&) = delete;

protected:
  int OversamplingFactor;
  double MinThreshold;
  double MaxThreshold;
};

#endif

// VTK-HeaderTest-Exclude: vtkGPUImageThresholdFilter.h
