/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkGPUSimpleImageFilter.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkGPUSimpleImageFilter
 * @brief   Help image algorithms use the GPU
 *
 * Designed to make it easier to accelerate an image algorithm on the GPU
*/

#ifndef vtkGPUSimpleImageFilter_h
#define vtkGPUSimpleImageFilter_h

#include "vtkAddon.h" // For export macro
#include "vtkObject.h"
#include "vtkAlgorithm.h"
#include <vtkOpenGLShaderProperty.h>

#include <vtkGPUAbstractImageFilter.h>

#include "vtkOpenGLHelper.h" // used for ivars
#include "vtkSmartPointer.h" // for ivar

#include <vector>

class VTK_ADDON_EXPORT vtkGPUSimpleImageFilter : public vtkGPUAbstractImageFilter
{
public:
  static vtkGPUSimpleImageFilter *New();
  vtkTypeMacro(vtkGPUSimpleImageFilter, vtkGPUAbstractImageFilter);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  vtkGetMacro(ShaderProperty, vtkOpenGLShaderProperty*);

  void SetOutputExtent(int extent[6]);
  vtkGetVector6Macro(OutputExtent, int);
  void ClearOutputExtent();

 protected:
  vtkGPUSimpleImageFilter();
  ~vtkGPUSimpleImageFilter() VTK_OVERRIDE;

 private:
  vtkGPUSimpleImageFilter(const vtkGPUSimpleImageFilter&) = delete;
  void operator=(const vtkGPUSimpleImageFilter&) = delete;
};

#endif

// VTK-HeaderTest-Exclude: vtkGPUSimpleImageFilter.h
