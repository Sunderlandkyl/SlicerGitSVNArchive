/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkGPUSimpleImageFilter.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include <vtkGPUSimpleImageFilter.h>
#include <vtkObjectFactory.h>

vtkStandardNewMacro(vtkGPUSimpleImageFilter);

// ----------------------------------------------------------------------------
vtkGPUSimpleImageFilter::vtkGPUSimpleImageFilter()
{
}

// ----------------------------------------------------------------------------
vtkGPUSimpleImageFilter::~vtkGPUSimpleImageFilter()
{
}

// ----------------------------------------------------------------------------
void vtkGPUSimpleImageFilter::SetOutputExtent(int extent[6])
{
  for (int i = 0; i < 6; ++i)
  {
    this->OutputExtent[i] = extent[i];
  }
  this->OutputExtentSpecified = true;
  this->Modified();
}

// ----------------------------------------------------------------------------
void vtkGPUSimpleImageFilter::ClearOutputExtent()
{
  for (int i = 0; i < 6; ++i)
  {
    this->OutputExtent[i] = (i % 2 == 0) ? 0 : -1;
  }
  this->OutputExtentSpecified = false;
  this->Modified();
}

// ----------------------------------------------------------------------------
void vtkGPUSimpleImageFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}