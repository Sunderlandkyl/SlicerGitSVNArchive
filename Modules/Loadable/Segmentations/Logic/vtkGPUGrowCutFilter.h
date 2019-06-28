/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkGPUGrowCutFilter.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkGPUGrowCutFilter
 * @brief   Help image algorithms use the GPU
 *
 * Designed to make it easier to accelerate an image algorithm on the GPU
*/

#ifndef vtkGPUGrowCutFilter_h
#define vtkGPUGrowCutFilter_h

#include "vtkSlicerSegmentationsModuleLogicExport.h" // For export macro
#include "vtkGPUAbstractImageFilter.h"

class VTK_SLICER_SEGMENTATIONS_LOGIC_EXPORT vtkGPUGrowCutFilter : public vtkGPUAbstractImageFilter
{
public:
  static vtkGPUGrowCutFilter *New();
  vtkTypeMacro(vtkGPUGrowCutFilter, vtkGPUAbstractImageFilter);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  vtkGetMacro(MaxThreshold, double);
  vtkSetMacro(MaxThreshold, double);

  vtkGetMacro(MinThreshold, double);
  vtkSetMacro(MinThreshold, double);

  vtkGetMacro(OversamplingFactor, int);
  vtkSetMacro(OversamplingFactor, int);


protected:
  vtkGPUGrowCutFilter();
  ~vtkGPUGrowCutFilter() VTK_OVERRIDE;

  void UpdateCustomUniformsFragment() override;

private:
  vtkGPUGrowCutFilter(const vtkGPUGrowCutFilter&) = delete;
  void operator=(const vtkGPUGrowCutFilter&) = delete;

protected:
  int OversamplingFactor;
  double MinThreshold;
  double MaxThreshold;
};

#endif

// VTK-HeaderTest-Exclude: vtkGPUGrowCutFilter.h
