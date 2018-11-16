/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkGPUImageGaussianFilter.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkGPUImageGaussianFilter
 * @brief   Help image algorithms use the GPU
 *
 * Designed to make it easier to accelerate an image algorithm on the GPU
*/

#ifndef vtkGPUImageGaussianFilter_h
#define vtkGPUImageGaussianFilter_h

#include "vtkAddon.h" // For export macro
#include "vtkGPUAbstractImageFilter.h"

class VTK_ADDON_EXPORT vtkGPUImageGaussianFilter : public vtkGPUAbstractImageFilter
{
public:
  static vtkGPUImageGaussianFilter *New();
  vtkTypeMacro(vtkGPUImageGaussianFilter, vtkGPUAbstractImageFilter);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

 protected:
  vtkGPUImageGaussianFilter();
  ~vtkGPUImageGaussianFilter() VTK_OVERRIDE;

 private:
  vtkGPUImageGaussianFilter(const vtkGPUImageGaussianFilter&) = delete;
  void operator=(const vtkGPUImageGaussianFilter&) = delete;
};

#endif

// VTK-HeaderTest-Exclude: vtkGPUImageGaussianFilter.h
