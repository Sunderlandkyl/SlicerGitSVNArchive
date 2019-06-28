/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkGPUGrowCutFilter.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkGPUGrowCutFilter.h"
#include "vtkObjectFactory.h"
#include "vtkOpenGLShaderProperty.h"
#include "vtkUniforms.h"

vtkStandardNewMacro(vtkGPUGrowCutFilter);

//----------------------------------------------------------------------------
vtkGPUGrowCutFilter::vtkGPUGrowCutFilter()
{
  this->SetNumberOfOutputPorts(2);
  this->ShaderProperty->SetFragmentShaderCode(R"(
#define MAX_STRENGTH vec4(10000)
//uniform int iterations;
//uniform int iteration;
//VTK::AlgTexUniforms::Dec
//VTK::CustomUniforms::Dec
//VTK::Output::Dec
void main()
{
  vec3 interpolatedTextureCoordinate = vec3(tcoordVSOutput, zPos);
  ivec3 size = textureSize(inputTex0, 0);
  ivec3 texelIndex = ivec3(floor(interpolatedTextureCoordinate * vec3(size)));
  vec4 background = texelFetch(inputTex0, texelIndex, 0).r;
  if (iteration == 0) {
    if (background < vec4(10)) {
      gl_FragData[0] = vec4(30);
      gl_FragData[1] = MAX_STRENGTH;
    } else if (background > vec4(100)) {
      gl_FragData[0] = vec4(100);
      gl_FragData[1] = MAX_STRENGTH;
    } else {
      gl_FragData[0] = vec4(0);
      gl_FragData[1] = vec4(0);
    }
  } else {
    gl_FragData[0] = texelFetch(inputTex1, texelIndex, 0).r;
    gl_FragData[1] = texelFetch(inputTex2, texelIndex, 0).r;
    for (int k = -1; k <= 1; k++) {
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          if (i != 0 && j != 0 && k != 0) {
            ivec3 neighborIndex = texelIndex + ivec3(i,j,k);
            vec4 neighborBackground = texelFetch(inputTex0, neighborIndex, 0).r;
            vec4 neighborStrength = texelFetch(inputTex2, neighborIndex, 0).r;
            vec4 gl_FragData[1]Cost = abs(neighborBackground - background);
            vec4 takeoverStrength = neighborStrength - gl_FragData[1]Cost;
            if (takeoverStrength > gl_FragData[1]) {
              gl_FragData[1] = takeoverStrength;
              gl_FragData[0] = texelFetch(inputTex1, neighborIndex, 0).r;
            }
          }
        }
      }
    }
  }
}
)");
}

//----------------------------------------------------------------------------
void vtkGPUGrowCutFilter::UpdateCustomUniformsFragment()
{
  vtkUniforms* fragmentUniforms = this->ShaderProperty->GetFragmentCustomUniforms();
  fragmentUniforms->SetUniformi("oversamplingFactor", this->OversamplingFactor);
  fragmentUniforms->SetUniformf("maxThreshold", this->MaxThreshold);
  fragmentUniforms->SetUniformf("minThreshold", this->MinThreshold);
}

//----------------------------------------------------------------------------
vtkGPUGrowCutFilter::~vtkGPUGrowCutFilter()
{
}

//----------------------------------------------------------------------------
void vtkGPUGrowCutFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}
