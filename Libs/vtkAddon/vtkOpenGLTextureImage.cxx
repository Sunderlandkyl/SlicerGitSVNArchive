/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkOpenGLTextureImage.cxx,v $

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkOpenGLTextureImage.h"

#include "vtkDataArray.h"
#include "vtkImageData.h"
#include "vtkMapper.h"
#include "vtkMatrix4x4.h"
#include "vtkObjectFactory.h"
#include "vtkOpenGLError.h"
#include "vtkOpenGLRenderer.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"

#include <vtkImageViewer.h>
#include <vtkRenderWindowInteractor.h>

#include <math.h>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkOpenGLTextureImage);

//----------------------------------------------------------------------------
vtkOpenGLTextureImage::vtkOpenGLTextureImage()
{
  this->ShaderComputation = NULL;
  this->ImageData = NULL;
  this->TextureName = 0;
  this->Interpolate = 0;
  this->TextureMTime = 0;
  this->TextureWrap = vtkOpenGLTextureImage::ClampToEdge;
}

//----------------------------------------------------------------------------
vtkOpenGLTextureImage::~vtkOpenGLTextureImage()
{
  this->SetShaderComputation(NULL);
  this->SetImageData(NULL);
}

//----------------------------------------------------------------------------
// adapted from Rendering/OpenGL2/vtkTextureObject.cxx
int vtkOpenGLTextureImage::vtkScalarTypeToGLType(int vtk_scalar_type)
{
  // DON'T DEAL with VTK_CHAR as this is platform dependent.
  switch (vtk_scalar_type)
    {
  case VTK_SIGNED_CHAR:
    return GL_BYTE;

  case VTK_UNSIGNED_CHAR:
    return GL_UNSIGNED_BYTE;

  case VTK_SHORT:
    return GL_SHORT;

  case VTK_UNSIGNED_SHORT:
    return GL_UNSIGNED_SHORT;

  case VTK_INT:
    return GL_INT;

  case VTK_UNSIGNED_INT:
    return GL_UNSIGNED_INT;

  case VTK_FLOAT:
  case VTK_VOID: // used for depth component textures.
    return GL_FLOAT;
    }
  return 0;
}

//----------------------------------------------------------------------------
// Reload the texture if needed
//
bool vtkOpenGLTextureImage::UpdateTexture()
{
  if (!this->ShaderComputation || !this->ShaderComputation->GetInitialized())
    {
    vtkErrorMacro("No initialized ShaderComputation instance is set.");
    return false;
    }
  this->ShaderComputation->MakeCurrent();

  if (this->ImageData->GetMTime() > this->TextureMTime)
    {
    if (this->TextureName != 0)
      {
      glDeleteTextures (1, &(this->TextureName) );
      }
    this->TextureMTime = 0;
    }
  else
    {
    return true;
    }

  if ( this->ImageData == 0 )
    {
    return false;
    }
  int componentCount = this->ImageData->GetNumberOfScalarComponents();
  GLenum format;
  GLenum internalFormat;
  if ( componentCount == 1 )
    {
    format = GL_RED;
    internalFormat = GL_RED;
    }
  else if ( componentCount == 3 )
    {
    format = GL_RGB;
    internalFormat = GL_RGB;
    }
  else if ( componentCount == 4 )
    {
    format = GL_RGBA;
    internalFormat = GL_RGBA;
    }
  else
    {
    vtkErrorMacro("Must have 1, 3 or 4 component image data for texture");
    return false;
    }

  int dimensions[3];
  this->ImageData->GetDimensions(dimensions);
  vtkPointData *pointData = this->ImageData->GetPointData();
  vtkDataArray *scalars = pointData->GetScalars();
  void *pixels = scalars->GetVoidPointer(0);

  vtkOpenGLCheckErrorMacro("before uploading");

  glGenTextures(1, &(this->TextureName));
  glBindTexture(GL_TEXTURE_3D, this->TextureName);
  if (this->Interpolate)
    {
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
  else
    {
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

  if (this->TextureWrap == vtkOpenGLTextureImage::MirroredRepeat)
    {
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    }
  else
    {
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

  glTexImage3D(
               /* target */            GL_TEXTURE_3D,
               /* level */             0,
               /* internal format */   internalFormat,
               /* width */             dimensions[0],
               /* height */            dimensions[1],
               /* depth */             dimensions[2],
               /* border */            0,
               /* format */            format,
               /* type */              vtkScalarTypeToGLType(this->ImageData->GetScalarType()),
               /* pixels */            pixels
  );
  vtkOpenGLCheckErrorMacro("after uploading");

  this->TextureMTime = this->GetMTime();
  return true;
}

//----------------------------------------------------------------------------
void vtkOpenGLTextureImage::Activate(vtkTypeUInt32 unit)
{

  vtkOpenGLCheckErrorMacro("before activating");

  if (!this->ShaderComputation || !this->ShaderComputation->GetInitialized())
    {
    vtkErrorMacro("No initialized ShaderComputation instance is set.");
    return;
    }
  this->ShaderComputation->MakeCurrent();

  // TODO: check the actual number (also expose way to check the
  // number from a wrapped language).  For now use the minimum max value.
  // of the enums, which only go to 15 even though 48 are meant to be
  // supported according to the OpenGL spec.
  // glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texture_units)
  #define __TEXTURE_UNIT_COUNT 16
  if (unit > __TEXTURE_UNIT_COUNT-1)
    {
    vtkErrorMacro("Only " << __TEXTURE_UNIT_COUNT << " texture units are available.");
    return;
    }

  if (!this->UpdateTexture())
    {
    vtkErrorMacro("Could not update texture.");
    return;
    }

  // TODO:
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_3D, this->TextureName);

  vtkOpenGLCheckErrorMacro("after activating");
}

//----------------------------------------------------------------------------
void vtkOpenGLTextureImage::AttachAsDrawTarget(int attachmentIndex, int layer, int attachment)
{
  if (!this->ShaderComputation || !this->ShaderComputation->GetInitialized())
    {
    vtkErrorMacro("No initialized ShaderComputation instance is set.");
    return;
    }
  this->ShaderComputation->MakeCurrent();

  vtkOpenGLCheckErrorMacro("before attaching");

  // attachment is 0 (color), 1 (depth), 2 (stencil), 3 (depth-stencil)
  if (attachmentIndex != 0 || attachment != 0)
    {
    vtkErrorMacro("Only GL_COLOR_ATTACHMENT0 supported for now.");
    return;
    }

  if (this->TextureName == 0)
    {
    if (!this->UpdateTexture())
      {
      vtkErrorMacro("Could not update texture.");
      return;
      }
    }

  vtkOpenGLClearErrorMacro();

  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  int dimensions[3] = {0,0,0};
  this->ImageData->GetDimensions(dimensions);

  //
  // Set up a normalized rendering environment
  //
  glViewport(0, 0, dimensions[0], dimensions[1]);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);

  glBindTexture(GL_TEXTURE_3D, this->TextureName);
  glFramebufferTextureLayer(
    /* target */      GL_FRAMEBUFFER,
    /* attachment */  GL_COLOR_ATTACHMENT0,
    /* texture */     this->TextureName,
    /* level */       0,
    /* layer */       layer);

  vtkOpenGLCheckErrorMacro("after attaching");

  //
  // Does the GPU support current Framebuffer configuration?
  //
  GLenum status;
  status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  switch(status)
    {
    case GL_FRAMEBUFFER_COMPLETE:
      break;
    default:
      vtkOpenGLCheckErrorMacro("after bad framebuffer status");
      vtkErrorMacro("Bad framebuffer configuration, status is: " << status);
      return;
    }
}

//----------------------------------------------------------------------------
void vtkOpenGLTextureImage::ReadBack()
{

  vtkOpenGLCheckErrorMacro("before getting");

  if (!this->ShaderComputation || !this->ShaderComputation->GetInitialized())
    {
    vtkErrorMacro("No initialized ShaderComputation instance is set.");
    return;
    }
  this->ShaderComputation->MakeCurrent();

  int componentCount = this->ImageData->GetNumberOfScalarComponents();
  GLenum format;
  if ( componentCount == 1 )
    {
    format = GL_RED;
    }
  else if (componentCount == 3)
    {
    format = GL_RGB;
    }
  else if ( componentCount == 4 )
    {
    format = GL_RGBA;
    }
  else
    {
    vtkErrorMacro("Must have 1, 3 or 4 component image data for texture");
    return;
    }

  //vtkPointData *pointData = this->ImageData->GetPointData();
  //vtkDataArray *scalars = pointData->GetScalars();
  //void *pixels = scalars->GetVoidPointer(0);
  void *pixels = this->ImageData->GetScalarPointer();

  // TODO:
  glBindTexture(GL_TEXTURE_3D, this->TextureName);

  glGetTexImage(
    /* target */ GL_TEXTURE_3D,
    /* level */  0,
    /* format */ format,
    /* type */   vtkScalarTypeToGLType(this->ImageData->GetScalarType()),
    /* pixels */ pixels);

  this->ImageData->Modified();

  //pixels = this->ImageData->GetScalarPointer();
  //unsigned short* q = (unsigned short*)pixels;
  //int numVoxels = this->GetImageData()->GetDimensions()[0]* this->GetImageData()->GetDimensions()[1] * this->GetImageData()->GetDimensions()[2];
  //for (unsigned long i = 0; i < numVoxels; ++i)
  //{
  //  float out = i / (float)(numVoxels);
  //  if (out > 0.5)
  //    int i = 0;

  //  (*q) = out * VTK_UNSIGNED_SHORT_MAX;
  //  ++q;
  //}

  //vtkRenderWindowInteractor* renderWindowInteractor = vtkRenderWindowInteractor::New();
  //vtkImageViewer* imageViewer = vtkImageViewer::New();
  //imageViewer->SetInputData(this->ImageData);
  //imageViewer->SetColorLevel(VTK_UNSIGNED_SHORT_MAX/2.0);
  //imageViewer->SetColorWindow(VTK_UNSIGNED_SHORT_MAX);
  //imageViewer->SetPosition(20, 600);
  //imageViewer->SetupInteractor(renderWindowInteractor);
  //imageViewer->Render();

  vtkOpenGLCheckErrorMacro("after getting");
}

//----------------------------------------------------------------------------
void vtkOpenGLTextureImage::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  if ( this->ShaderComputation )
    {
    os << indent << "ShaderComputation: " << this->ShaderComputation << "\n";
    }
  else
    {
    os << indent << "ShaderComputation: (none)\n";
    }
  if ( this->ImageData )
    {
    os << indent << "ImageData: " << this->ImageData << "\n";
    }
  else
    {
    os << indent << "ImageData: (none)\n";
    }
  os << indent << "TextureName: " << this->TextureName << "\n";
  os << indent << "TextureMTime: " << this->TextureMTime << "\n";
}
