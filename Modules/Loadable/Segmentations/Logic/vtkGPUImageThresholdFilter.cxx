/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkGPUImageThresholdFilter.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkGPUImageThresholdFilter.h"
#include "vtkObjectFactory.h"
#include "vtkOpenGLShaderProperty.h"
#include "vtkUniforms.h"

vtkStandardNewMacro(vtkGPUImageThresholdFilter);

//----------------------------------------------------------------------------
vtkGPUImageThresholdFilter::vtkGPUImageThresholdFilter()
{
  this->ShaderProperty->SetFragmentShaderCode(R"(
//VTK::System::Dec
varying vec2 tcoordVSOutput;
uniform float zPos;
//VTK::AlgTexUniforms::Dec
//VTK::CustomUniforms::Dec
//VTK::Output::Dec
void main()
{
// Can't have an oversampling factor that is less than zero.
gl_FragData[0] = vec4(vec3(0.), 1.0);
if (oversamplingFactor > 0.0)
  {
  float offsetStart = -(oversamplingFactor - 1)/(2 * oversamplingFactor);
  float stepSize = 1.0/oversamplingFactor;
  float sum = 0;

  float scaledMin = max(minThreshold / (inputScale0 + inputShift0), -1.0);
  float scaledMax = max(maxThreshold / (inputScale0 + inputShift0), -1.0);

  // Iterate over 216 offset points.
  for (int k = 0; k < oversamplingFactor; ++k)
    {
    for (int j = 0; j < oversamplingFactor; ++j)
      {
      for (int i = 0; i < oversamplingFactor; ++i)
        {

        // Calculate the current offset.
        vec3 offset = vec3(
          (offsetStart + stepSize*i)/(inputSize0.x),
          (offsetStart + stepSize*j)/(inputSize0.y),
          (offsetStart + stepSize*k)/(inputSize0.z));

        vec3 offsetTextureCoordinate = vec3(tcoordVSOutput, zPos) + offset;

        // If the value of the interpolated offset pixel is greater than the threshold, then
        // increment the fractional sum.
        vec4 referenceSample = texture(inputTex0, offsetTextureCoordinate);
        if (referenceSample.r >= scaledMin && referenceSample.r <= scaledMax )
          {
          ++sum;
          }
        }
      }
    }
  // Calculate the fractional value of the pixel.
  sum = sum - 108;
  sum = sum / (outputScale0 + outputShift0);
  gl_FragData[0] = vec4( vec3(sum), 1.0 );
  }
}
)");
}

//----------------------------------------------------------------------------
void vtkGPUImageThresholdFilter::UpdateCustomUniformsFragment()
{
  vtkUniforms* fragmentUniforms = this->ShaderProperty->GetFragmentCustomUniforms();
  fragmentUniforms->SetUniformi("oversamplingFactor", this->OversamplingFactor);
  fragmentUniforms->SetUniformf("maxThreshold", this->MaxThreshold);
  fragmentUniforms->SetUniformf("minThreshold", this->MinThreshold);
}

//----------------------------------------------------------------------------
vtkGPUImageThresholdFilter::~vtkGPUImageThresholdFilter()
{
}

//----------------------------------------------------------------------------
void vtkGPUImageThresholdFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
