/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkGPUAbstractImageFilter.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkGPUAbstractImageFilter.h"

#include "vtk_glew.h"
#include "vtkCommand.h"
#include "vtkDemandDrivenPipeline.h"
#include "vtkEventForwarderCommand.h"
#include "vtkGPUImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkOpenGLFramebufferObject.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkOpenGLShaderCache.h"
#include "vtkOpenGLShaderProperty.h"
#include "vtkOpenGLState.h"
#include "vtkOpenGLUniforms.h"
#include "vtkOpenGLVertexArrayObject.h"
#include "vtkPixelTransfer.h"
#include "vtkShaderProgram.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkTextureObject.h"
#include "vtkOpenGLError.h"
#include <sstream>

vtkStandardNewMacro(vtkGPUAbstractImageFilter);

// ----------------------------------------------------------------------------
vtkGPUAbstractImageFilter::vtkGPUAbstractImageFilter()
{
  // Basic vertex shader
  this->DefaultVertexShaderSource = R"(
//VTK::System::Dec
attribute vec4 vertexMC;
attribute vec2 tcoordMC;
varying vec2 tcoordVSOutput;
void main()
{
  tcoordVSOutput = tcoordMC;
  gl_Position = vertexMC;
}
)";

  // Pass-through shader
  this->DefaultFragmentShaderSource = R"(
//VTK::System::Dec
in vec2 tcoordVC;
uniform sampler2D source;
//VTK::Output::Dec
void main(void)
{
  gl_FragData[0] = texture2D(source, tcoordVC);
}
)";

  for (int i = 0; i < 3; ++i)
  {
    this->OutputExtent[i] = 0;
    this->OutputExtent[i+1] = -1;
  }

  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
  this->OutputScalarType = VTK_FLOAT;
  this->RenderWindow = nullptr;
  this->OutputExtentSpecified = false;
  this->NumberOfComponents = 1;

  this->ShaderProperty->AddObserver(vtkCommand::ModifiedEvent, this, &vtkGPUAbstractImageFilter::ShaderPropertyModifed);
  this->ShaderProperty->GetFragmentCustomUniforms()->AddObserver(vtkCommand::ModifiedEvent, this, &vtkGPUAbstractImageFilter::ShaderPropertyModifed);
  this->ShaderProperty->GetVertexCustomUniforms()->AddObserver(vtkCommand::ModifiedEvent, this, &vtkGPUAbstractImageFilter::ShaderPropertyModifed);
}

// ----------------------------------------------------------------------------
vtkGPUAbstractImageFilter::~vtkGPUAbstractImageFilter()
{
  this->SetRenderWindow(nullptr);
}

//-----------------------------------------------------------------------------
void vtkGPUAbstractImageFilter::ShaderPropertyModifed(vtkObject*, unsigned long, void*)
{
  this->Modified();
}

// ----------------------------------------------------------------------------
void vtkGPUAbstractImageFilter::SetRenderWindow(vtkRenderWindow *renWin)
{
  if (renWin == this->RenderWindow.GetPointer())
  {
    return;
  }

  vtkOpenGLRenderWindow *orw  = nullptr;
  if (renWin)
  {
    orw = vtkOpenGLRenderWindow::SafeDownCast(renWin);
  }

  this->RenderWindow = orw;
  this->Modified();
}

// ----------------------------------------------------------------------------
void vtkGPUAbstractImageFilter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "RenderWindow:";
  if(this->RenderWindow != nullptr)
  {
    this->RenderWindow->PrintSelf(os,indent);
  }
  else
  {
    os << "(none)" <<endl;
  }
}

// ----------------------------------------------------------------------------
int vtkGPUAbstractImageFilter::FillInputPortInformation(int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_IS_REPEATABLE(), 1);
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkGPUImageData");
  if (this->RenderWindow)
  {
    info->Set(vtkGPUImageData::CONTEXT_OBJECT(), this->RenderWindow);
  }
  return 1;
}

//----------------------------------------------------------------------------
int vtkGPUAbstractImageFilter::FillOutputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  // now add our info
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkGPUImageData");
  info->Set(vtkGPUImageData::CONTEXT_OBJECT(), this->RenderWindow);
  if (this->RenderWindow)
  {
    info->Set(vtkGPUImageData::CONTEXT_OBJECT(), this->RenderWindow);
  }
  return 1;
}

//----------------------------------------------------------------------------
int vtkGPUAbstractImageFilter::ProcessRequest(vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{

  // generate the data
  if (request->Has(vtkDemandDrivenPipeline::REQUEST_DATA_OBJECT()))
  {
    return this->RequestDataObject(request, inputVector, outputVector);
  }

  // generate the data
  if (request->Has(vtkDemandDrivenPipeline::REQUEST_DATA()))
  {
    return this->RequestData(request, inputVector, outputVector);
  }

  // execute information
  if (request->Has(vtkDemandDrivenPipeline::REQUEST_INFORMATION()))
  {
    return this->RequestInformation(request, inputVector, outputVector);
  }

  // propagate update extent
  if (request->Has(vtkStreamingDemandDrivenPipeline::REQUEST_UPDATE_EXTENT()))
  {
    return this->RequestUpdateExtent(request, inputVector, outputVector);
  }

  return this->Superclass::ProcessRequest(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
int vtkGPUAbstractImageFilter::RequestInformation(
  vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  if (this->OutputExtentSpecified)
  {
    outInfo->Set(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), this->OutputExtent, 6);
  }

  for (int idx = 0; idx < this->GetNumberOfInputConnections(0); ++idx)
  {
    vtkInformation *inInfo = inputVector[0]->GetInformationObject(idx);
    if (!this->RenderWindow && inInfo && inInfo->Has(vtkGPUImageData::CONTEXT_OBJECT()))
    {
      this->SetRenderWindow(vtkOpenGLRenderWindow::SafeDownCast(inInfo->Get(vtkGPUImageData::CONTEXT_OBJECT())));
    }
  }
  if (!this->RenderWindow && outInfo && outInfo->Has(vtkGPUImageData::CONTEXT_OBJECT()))
  {
    this->SetRenderWindow(vtkOpenGLRenderWindow::SafeDownCast(outInfo->Get(vtkGPUImageData::CONTEXT_OBJECT())));
  }

  if (!this->RenderWindow)
  {
    this->SetRenderWindow(vtkSmartPointer<vtkRenderWindow>::New());
    this->RenderWindow->SetOffScreenRendering(true);
    this->RenderWindow->Initialize();
  }

  if (this->RenderWindow)
  {
    for (int idx = 0; idx < this->GetNumberOfInputConnections(0); ++idx)
    {
      vtkInformation *inInfo = inputVector[0]->GetInformationObject(idx);
      if (inInfo)
      {
        inInfo->Set(vtkGPUImageData::CONTEXT_OBJECT(), this->RenderWindow);
      }
    }

    if (outInfo)
    {
      outInfo->Set(vtkGPUImageData::CONTEXT_OBJECT(), this->RenderWindow);
    }
  }

  return 1;
}

//----------------------------------------------------------------------------
int vtkGPUAbstractImageFilter::RequestDataObject(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** vtkNotUsed(inputVector),
  vtkInformationVector* outputVector)
{
  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  vtkGPUImageData *output = vtkGPUImageData::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));
  if (!output)
  {
    vtkGPUImageData* newOutput = vtkGPUImageData::New();
    outInfo->Set(vtkDataObject::DATA_OBJECT(), newOutput);
    newOutput->Delete();
  }
  return 1;
}

//----------------------------------------------------------------------------
int vtkGPUAbstractImageFilter::RequestUpdateExtent(
  vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector,
  vtkInformationVector* vtkNotUsed(outputVector))
{
  for (int idx = 0; idx < this->GetNumberOfInputConnections(0); ++idx)
  {
    vtkInformation *inInfo = inputVector[0]->GetInformationObject(idx);
    int ext[6];
    inInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), ext);
    inInfo->Set(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(), ext, 6);
  }
  return 1;
}

//----------------------------------------------------------------------------
int vtkGPUAbstractImageFilter::RequestData(
  vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  int outputPort = request->Get(vtkDemandDrivenPipeline::FROM_OUTPUT_PORT());
  if (outputPort == -1)
  {
    outputPort = 0;
  }

  vtkInformation *outInfo = outputVector->GetInformationObject(outputPort);
  vtkGPUImageData *outputGPUImage = vtkGPUImageData::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));

  std::vector<vtkGPUImageData*> inputGPUImages;
  for (int idx = 0; idx < this->GetNumberOfInputConnections(0); ++idx)
  {
    vtkInformation *inInfo = inputVector[0]->GetInformationObject(idx);
    vtkGPUImageData* inputGPUImage = vtkGPUImageData::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));
    if (!inputGPUImage)
    {
      vtkErrorMacro("Input GPU image " << idx << " is missing");
      return 0;
    }

    if (!inputGPUImage->GetContext() || inputGPUImage->GetContext() != this->RenderWindow)
    {
      vtkErrorMacro("Input texture has invalid or missing rendering context");
      return 0;
    }

    inputGPUImages.push_back(inputGPUImage);
  }

  // TODO: extent calculation
  int outExtent[6];
  if (outInfo->Has(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT()))
  {
    outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(), outExtent);
  }
  outputGPUImage->SetExtent(outExtent);
  outputGPUImage->SetContext(this->RenderWindow);
  if (!outputGPUImage->AllocateScalars(this->OutputScalarType, this->NumberOfComponents))
  {
    return 0;
  }

  return this->Execute(inputGPUImages, outputGPUImage);
}

//----------------------------------------------------------------------------
int vtkGPUAbstractImageFilter::Execute(std::vector<vtkGPUImageData*> inputGPUImages, vtkGPUImageData* outputGPUImage)
{
  if (this->ShaderRebuildNeeded())
  {
    this->BuildShader(inputGPUImages, outputGPUImage);
  }

  if (!this->ShaderProgram)
  {
    vtkErrorMacro("Error building shader program");
    return 0;
  }

  this->RenderWindow->GetShaderCache()->ReadyShaderProgram(this->ShaderProgram);

  // now create the framebuffer for the output
  int outputDimensions[3] = { 0, 0, 0 };
  outputGPUImage->GetDimensions(outputDimensions);

  int outputExtent[6] = { 0, -1, 0, -1, 0, -1 };
  outputGPUImage->GetExtent(outputExtent);

  vtkNew<vtkOpenGLFramebufferObject> fbo;
  fbo->SetContext(this->RenderWindow);
  vtkOpenGLState *ostate = this->RenderWindow->GetState();
  fbo->AddColorAttachment(fbo->GetDrawMode(), 0, outputGPUImage->GetTextureObject(), 0);
  fbo->ActivateDrawBuffer(0);
  fbo->StartNonOrtho(outputDimensions[0], outputDimensions[1]);
  ostate->vtkglViewport(0, 0, outputDimensions[0], outputDimensions[1]);
  ostate->vtkglScissor(0, 0, outputDimensions[0], outputDimensions[1]);
  ostate->vtkglDisable(GL_DEPTH_TEST);

  // Upload the value of user-defined uniforms in the program
  vtkOpenGLUniforms* vertexUniforms = static_cast<vtkOpenGLUniforms*>(this->ShaderProperty->GetVertexCustomUniforms());
  vertexUniforms->SetUniforms(this->ShaderProgram);
  vtkOpenGLUniforms* fragmentUniforms = static_cast<vtkOpenGLUniforms*>(this->ShaderProperty->GetFragmentCustomUniforms());
  fragmentUniforms->SetUniforms(this->ShaderProgram);
  if (this->ShaderProgram != this->Quad.Program)
  {
    this->Quad.Program = this->ShaderProgram;
    this->Quad.VAO->ShaderProgramChanged();
  }

  ////////////////////////////////
  // Manage input textures
  for (int textureIndex = 0; textureIndex < inputGPUImages.size(); ++textureIndex)
  {
    vtkGPUImageData* inputGPUImageData = inputGPUImages[textureIndex];
    vtkTextureObject* inputTexture = inputGPUImageData->GetTextureObject();
    inputTexture->Activate();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }

  this->UpdateTextureUniforms(inputGPUImages, outputGPUImage);

  ////////////////////////////////
  // Render

  // For each zslice in the output
  vtkPixelExtent outputPixelExt(outputExtent);
  for (int i = outputExtent[4]; i <= outputExtent[5]; ++i)
  {
    int textureSliceIndex = i - outputExtent[4];

    this->Quad.Program->SetUniformf("zPos", (textureSliceIndex + 0.5) / outputDimensions[2]);
    fbo->RemoveColorAttachment(fbo->GetDrawMode(), 0);
    fbo->AddColorAttachment(fbo->GetDrawMode(), 0, outputGPUImage->GetTextureObject(), textureSliceIndex);

    const char *error;
    if (!vtkOpenGLFramebufferObject::GetFrameBufferStatus(fbo->GetDrawMode(), error))
    {
      vtkErrorMacro("Invalid framebuffer status: " << error);
      return 0;
    }

    fbo->RenderQuad(
      0, outputDimensions[0] - 1,
      0, outputDimensions[1] - 1,
      this->Quad.Program, this->Quad.VAO);
  }

  return 1;
}

//----------------------------------------------------------------------------
void vtkGPUAbstractImageFilter::UpdateTextureUniforms(std::vector<vtkGPUImageData*> inputGPUImages, vtkGPUImageData* outputGPUImage)
{
  float outputShift = 0.0;
  float outputScale = 1.0;
  outputGPUImage->GetTextureObject()->GetShiftAndScale(outputShift, outputScale);

  // Provide shift and scale to get the data backing into its original units
  std::stringstream outputShiftSS;
  outputShiftSS << "outputShift";
  std::string outputShiftString = outputShiftSS.str();
  this->Quad.Program->SetUniformf(outputShiftString.c_str(), outputShift);

  std::stringstream outputScaleSS;
  outputScaleSS << "outputScale";
  std::string outputScaleString = outputScaleSS.str();
  this->Quad.Program->SetUniformf(outputScaleString.c_str(), outputScale);

  int outputDimensions[3] = { 0,0,0 };
  outputGPUImage->GetDimensions(outputDimensions);
  float outputDimensionsFloat[3] = { (float)outputDimensions[0], (float)outputDimensions[1], (float)outputDimensions[2] };
  std::stringstream outputSizeSS;
  outputSizeSS << "outputSize";
  std::string outputSizeString = outputSizeSS.str();
  this->Quad.Program->SetUniform3f(outputSizeString.c_str(), outputDimensionsFloat);

  double outputSpacing[3] = { 0,0,0 };
  outputGPUImage->GetSpacing(outputSpacing);
  float outputSpacingFloat[3] = { (float)outputSpacing[0], (float)outputSpacing[1], (float)outputSpacing[2] };
  std::stringstream outputSpacingSS;
  outputSpacingSS << "outputSpacing";
  std::string outputSpacingString = outputSpacingSS.str();
  this->Quad.Program->SetUniform3f(outputSpacingString.c_str(), outputSpacingFloat);

  ////////////////////////////////
  // Manage input textures
  for (int inputImageIndex = 0; inputImageIndex < inputGPUImages.size(); ++inputImageIndex)
  {
    vtkGPUImageData* inputGPUImage = inputGPUImages[inputImageIndex];
    float shift = 0.0;
    float scale = 1.0;
    inputGPUImage->GetTextureObject()->GetShiftAndScale(shift, scale);
    int inputTexId = inputGPUImage->GetTextureObject()->GetTextureUnit();

    // Set Texture uniforms
    std::stringstream texIdSS;
    texIdSS << "inputTex" << inputImageIndex;
    std::string texId = texIdSS.str();
    this->Quad.Program->SetUniformi(texId.c_str(), inputTexId);

    // Provide shift and scale to get the data backing into its original units
    std::stringstream inputShiftSS;
    inputShiftSS << "inputShift" << inputImageIndex;
    std::string inputShiftString = inputShiftSS.str();
    this->Quad.Program->SetUniformf(inputShiftString.c_str(), shift);

    std::stringstream inputScaleSS;
    inputScaleSS << "inputScale" << inputImageIndex;
    std::string inputScaleString = inputScaleSS.str();
    this->Quad.Program->SetUniformf(inputScaleString.c_str(), scale);

    int inputTextureDimensions[3] = { 0,0,0 };
    inputGPUImage->GetDimensions(inputTextureDimensions);
    float inputTextureDimensionsFloat[3] = { (float)inputTextureDimensions[0], (float)inputTextureDimensions[1], (float)inputTextureDimensions[2]};
    std::stringstream inputSizeSS;
    inputSizeSS << "inputSize" << inputImageIndex;
    std::string inputSizeString = inputSizeSS.str();
    this->Quad.Program->SetUniform3f(inputSizeString.c_str(), inputTextureDimensionsFloat);

    double inputSpacing[3] = { 0,0,0 };
    inputGPUImage->GetSpacing(inputSpacing);
    float inputSpacingFloat[3] = { (float)inputSpacing[0], (float)inputSpacing[1], (float)inputSpacing[2] };
    std::stringstream inputSpacingSS;
    inputSpacingSS << "inputSpacing" << inputImageIndex;
    std::string inputSpacingString = inputSpacingSS.str();
    this->Quad.Program->SetUniform3f(inputSpacingString.c_str(), inputSpacingFloat);
  }
}

//----------------------------------------------------------------------------
vtkGPUImageData* vtkGPUAbstractImageFilter::GetOutput()
{
  return this->GetOutput(0);
}

//----------------------------------------------------------------------------
void vtkGPUAbstractImageFilter::SetOutput(vtkDataObject* d)
{
  this->GetExecutive()->SetOutputData(0, d);
}

//----------------------------------------------------------------------------
vtkGPUImageData* vtkGPUAbstractImageFilter::GetOutput(int port)
{
  return vtkGPUImageData::SafeDownCast(this->GetOutputDataObject(port));
}

//----------------------------------------------------------------------------
vtkGPUImageData* vtkGPUAbstractImageFilter::GetInput(int port)
{
  return vtkGPUImageData::SafeDownCast(this->GetInputDataObject(port, 0));
}

//----------------------------------------------------------------------------
void vtkGPUAbstractImageFilter::AddInputData(vtkGPUImageData* input)
{
  this->AddInputDataInternal(0, input);
}

//----------------------------------------------------------------------------
bool vtkGPUAbstractImageFilter::ShaderRebuildNeeded()
{
  return !this->ShaderProgram || this->ShaderBuildTime.GetMTime() < this->ShaderProperty->GetMTime();
}

//----------------------------------------------------------------------------
void vtkGPUAbstractImageFilter::BuildShader(std::vector<vtkGPUImageData*> inputTextures, vtkGPUImageData* outputTexture)
{
  std::map<vtkShader::Type, vtkShader*> shaders;
  vtkSmartPointer<vtkShader> vertexShader = vtkSmartPointer<vtkShader>::New();
  vertexShader->SetType(vtkShader::Vertex);
  shaders[vtkShader::Vertex] = vertexShader;

  vtkSmartPointer<vtkShader> fragmentShader = vtkSmartPointer<vtkShader>::New();
  fragmentShader->SetType(vtkShader::Fragment);
  shaders[vtkShader::Fragment] = fragmentShader;

  vtkSmartPointer<vtkShader> geometryShader = vtkSmartPointer<vtkShader>::New();
  geometryShader->SetType(vtkShader::Geometry);
  shaders[vtkShader::Geometry] = geometryShader;

  if (shaders[vtkShader::Vertex])
  {
    if (this->ShaderProperty->HasVertexShaderCode())
    {
      shaders[vtkShader::Vertex]->SetSource(this->ShaderProperty->GetVertexShaderCode());
    }
    else
    {
      shaders[vtkShader::Vertex]->SetSource(this->DefaultVertexShaderSource);
    }
  }

  if (shaders[vtkShader::Fragment])
  {
    if (this->ShaderProperty->HasFragmentShaderCode())
    {
      shaders[vtkShader::Fragment]->SetSource(this->ShaderProperty->GetFragmentShaderCode());
    }
    else
    {
      shaders[vtkShader::Fragment]->SetSource(this->DefaultFragmentShaderSource);
    }
  }

  //TODO
  if (shaders[vtkShader::Geometry])
  {
    shaders[vtkShader::Geometry]->SetSource(this->DefaultGeometryShaderSource);
  }

  // user specified pre replacements
  vtkOpenGLShaderProperty::ReplacementMap repMap = this->ShaderProperty->GetAllShaderReplacements();
  for (auto i : repMap)
  {
    if (i.first.ReplaceFirst)
    {
      std::string ssrc = shaders[i.first.ShaderType]->GetSource();
      vtkShaderProgram::Substitute(ssrc,
        i.first.OriginalValue,
        i.second.Replacement,
        i.second.ReplaceAll);
      shaders[i.first.ShaderType]->SetSource(ssrc);
    }
  }

  // Custom uniform variables replacements
  //---------------------------------------------------------------------------
  this->ReplaceShaderCustomUniforms(shaders, this->ShaderProperty);

  // Custom uniform variables replacements
  //---------------------------------------------------------------------------
  this->ReplaceShaderTextureInput(shaders, inputTextures, outputTexture);

  // user specified post replacements
  for (auto i : repMap)
  {
    if (!i.first.ReplaceFirst)
    {
      std::string ssrc = shaders[i.first.ShaderType]->GetSource();
      vtkShaderProgram::Substitute(ssrc,
        i.first.OriginalValue,
        i.second.Replacement,
        i.second.ReplaceAll);
      shaders[i.first.ShaderType]->SetSource(ssrc);
    }
  }

  this->ShaderProgram = this->RenderWindow->GetShaderCache()->ReadyShaderProgram(shaders);
  if (!this->ShaderProgram)
  {
    vtkErrorMacro("Shader failed to compile");
  }
  this->ShaderBuildTime.Modified();
}

//-----------------------------------------------------------------------------
void vtkGPUAbstractImageFilter::ReplaceShaderCustomUniforms(
  std::map<vtkShader::Type, vtkShader*>& shaders, vtkOpenGLShaderProperty * p)
{
  vtkShader* vertexShader = shaders[vtkShader::Vertex];
  vtkOpenGLUniforms * vu = static_cast<vtkOpenGLUniforms*>(p->GetVertexCustomUniforms());
  vtkShaderProgram::Substitute(vertexShader,
    "//VTK::CustomUniforms::Dec",
    vu->GetDeclarations());

  vtkShader* fragmentShader = shaders[vtkShader::Fragment];
  vtkOpenGLUniforms * fu = static_cast<vtkOpenGLUniforms*>(p->GetFragmentCustomUniforms());
  vtkShaderProgram::Substitute(fragmentShader,
    "//VTK::CustomUniforms::Dec",
    fu->GetDeclarations());
}

//----------------------------------------------------------------------------
void vtkGPUAbstractImageFilter::ReplaceShaderTextureInput(std::map<vtkShader::Type, vtkShader*>& shaders,
  std::vector<vtkGPUImageData*> inputTextures, vtkGPUImageData* outputTexture)
{
  std::stringstream textureUniformReplacementSS;

  textureUniformReplacementSS << "uniform float outputShift;" << std::endl;
  textureUniformReplacementSS << "uniform float outputScale;" << std::endl;
  textureUniformReplacementSS << "uniform vec3 outputSize;" << std::endl;
  textureUniformReplacementSS << "uniform vec3 outputSpacing;" << std::endl;

  for (int textureIndex = 0; textureIndex < inputTextures.size(); ++textureIndex)
  {
    textureUniformReplacementSS << "uniform sampler3D inputTex" << textureIndex << ";" << std::endl;
    textureUniformReplacementSS << "uniform float inputShift" << textureIndex << ";" << std::endl;
    textureUniformReplacementSS << "uniform float inputScale" << textureIndex << ";" << std::endl;
    textureUniformReplacementSS << "uniform vec3 inputSize" << textureIndex << ";" << std::endl;
    textureUniformReplacementSS << "uniform vec3 inputSpacing" << textureIndex << ";" << std::endl;
  }

  for (std::map<vtkShader::Type, vtkShader*>::iterator shaderIt = shaders.begin(); shaderIt != shaders.end(); ++shaderIt)
  {
    vtkShader* shader = shaderIt->second;
    vtkShaderProgram::Substitute(shader,
      "//VTK::AlgTexUniforms::Dec",
      textureUniformReplacementSS.str());
  }
}