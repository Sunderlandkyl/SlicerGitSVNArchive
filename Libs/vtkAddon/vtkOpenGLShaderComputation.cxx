/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkOpenGLShaderComputation.cxx,v $

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkOpenGLShaderComputation.h"

#include "vtkOpenGLTextureImage.h"

#include "vtkDataArray.h"
#include "vtkImageData.h"
#include "vtkObjectFactory.h"
#include "vtkOpenGLError.h"
#include "vtkOpenGLExtensionManager.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkPointData.h"
#include "vtkRenderer.h"

#include "vtkOpenGL.h"
#include "vtkgl.h"

#include <math.h>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkOpenGLShaderComputation);

//----------------------------------------------------------------------------
vtkOpenGLShaderComputation::vtkOpenGLShaderComputation()
{
  this->Initialized = false;
  this->VertexShaderSource = NULL;
  this->FragmentShaderSource = NULL;
  this->ResultImageData = NULL;
  this->ProgramObject = 0;
  this->ProgramObjectMTime = 0;

  this->RenderWindow = vtkRenderWindow::New();
  this->RenderWindow->OffScreenRenderingOff();

  this->Initialize(this->RenderWindow);
}

//----------------------------------------------------------------------------
vtkOpenGLShaderComputation::~vtkOpenGLShaderComputation()
{
  this->MakeCurrent();
  //Bind 0, which means render to back buffer, as a result, this->FramebufferID is unbound
  vtkgl::BindFramebuffer(vtkgl::FRAMEBUFFER, 0);
  if (this->FramebufferID != 0)
    {
    vtkgl::DeleteFramebuffers(1, &(this->FramebufferID));
    }
  this->ReleaseResultRenderbuffer();
  this->SetVertexShaderSource(NULL);
  this->SetFragmentShaderSource(NULL);
  this->SetResultImageData(NULL);
  if (this->ProgramObject > 0)
    {
    vtkgl::DeleteObjectARB ( this->ProgramObject );
    this->ProgramObject = 0;
    }
  this->SetRenderWindow(NULL);
}

//----------------------------------------------------------------------------
// Make sure OpenGL calls are sent to our render context
//
void vtkOpenGLShaderComputation::MakeCurrent()
{

  if (this->RenderWindow)
    {
    this->RenderWindow->MakeCurrent();
    }
  else
    {
    vtkErrorMacro ( "Trying to make current but render window is null" );
    }
}

//----------------------------------------------------------------------------
///
// Create a shader object, load the shader source, and
// compile the shader.
//
static GLuint CompileShader ( vtkOpenGLShaderComputation *self, GLenum type, const char *shaderSource )
{
  self->MakeCurrent();
  vtkOpenGLClearErrorMacro();

  GLuint shader;
  GLint compiled;

  // Create the shader object
  shader = vtkgl::CreateShaderObjectARB ( type );

  if ( shader == 0 )
    {
    return 0;
    }

  // Load the shader source
  vtkgl::ShaderSourceARB( shader, 1, &shaderSource, NULL );

  // Compile the shader
  vtkgl::CompileShaderARB( shader );
  vtkOpenGLStaticCheckErrorMacro("after compiling shader");

  // Check the compile status
  vtkgl::GetObjectParameterivARB ( shader, vtkgl::COMPILE_STATUS, &compiled );
  if ( !compiled )
    {
    GLint infoLen = 0;
    vtkgl::GetObjectParameterivARB ( shader, vtkgl::INFO_LOG_LENGTH, &infoLen );
    if ( infoLen > 1 )
      {
      char *infoLog = (char *) malloc ( sizeof ( char ) * infoLen );
      vtkgl::GetShaderInfoLog ( shader, infoLen, NULL, infoLog );
      switch(type)
        {
        case vtkgl::VERTEX_SHADER:
          vtkErrorWithObjectMacro (self, "Error compiling vertex shader\n" << infoLog );
          break;
        case vtkgl::FRAGMENT_SHADER:
          vtkErrorWithObjectMacro (self, "Error compiling fragment shader\n" << infoLog );
          break;
        default:
          vtkErrorWithObjectMacro (self, "Error compiling unknown shader type!\n" << infoLog );
          break;
        }
      free ( infoLog );
      }
      vtkOpenGLStaticCheckErrorMacro("after checking compile status");
      vtkgl::DeleteShader ( shader );
      vtkOpenGLStaticCheckErrorMacro("after deleting bad shader");
      return 0;
    }

  vtkOpenGLStaticCheckErrorMacro("after compiling shader");
  return shader;
}

//----------------------------------------------------------------------------
// Rebuild the shader program if needed
//
bool vtkOpenGLShaderComputation::UpdateProgram()
{
  vtkOpenGLClearErrorMacro();
  GLuint vertexShader;
  GLuint fragmentShader;
  GLint linked;

  this->MakeCurrent();

  if (this->GetMTime() > this->ProgramObjectMTime)
    {
    if (this->ProgramObject != 0)
      {
      vtkgl::DeleteProgram ( this->ProgramObject );
      }
    this->ProgramObjectMTime = 0;
    }
  else
    {
    return true;
    }

  // Load the vertex/fragment shaders
  vertexShader = CompileShader ( this, vtkgl::VERTEX_SHADER, this->VertexShaderSource );
  fragmentShader = CompileShader ( this, vtkgl::FRAGMENT_SHADER, this->FragmentShaderSource );

  if ( !vertexShader || !fragmentShader )
    {
    vtkOpenGLCheckErrorMacro("after failed compile");
    return false;
    }

  // Create the program object
  this->ProgramObject = vtkgl::CreateProgramObjectARB ( );

  if ( this->ProgramObject == 0 )
    {
    vtkOpenGLCheckErrorMacro("after failed program create");
    return false;
    }

  vtkgl::AttachObjectARB ( this->ProgramObject, vertexShader );
  vtkgl::AttachObjectARB ( this->ProgramObject, fragmentShader );

  vtkgl::LinkProgramARB ( this->ProgramObject );

  // Check the link status
  vtkgl::GetObjectParameterivARB ( this->ProgramObject, vtkgl::LINK_STATUS, &linked );

  if ( !linked )
    {
    // something went wrong, so emit error message if possible
    GLint infoLen = 0;
    vtkgl::GetObjectParameterivARB ( this->ProgramObject, vtkgl::INFO_LOG_LENGTH, &infoLen );

    if ( infoLen > 1 )
      {
      char *infoLog = (char *) malloc ( sizeof ( char ) * infoLen );

      vtkgl::GetProgramInfoLog ( this->ProgramObject, infoLen, NULL, infoLog );
      vtkErrorMacro ( "Error linking program\n" << infoLog );

      free ( infoLog );
      }

    vtkgl::DeleteObjectARB ( this->ProgramObject );
    vtkOpenGLCheckErrorMacro("after failed program attachment");
    return false;
    }

  this->ProgramObjectMTime = this->GetMTime();
  vtkOpenGLCheckErrorMacro("after program creation");
  return true;
}

//-----------------------------------------------------------------------------
void vtkOpenGLShaderComputation::Initialize(vtkRenderWindow *renderWindow)
{
  if (this->Initialized)
    {
    return;
    }

  vtkOpenGLRenderWindow *openGLRenderWindow = vtkOpenGLRenderWindow::SafeDownCast(renderWindow);
  if (!openGLRenderWindow)
    {
    vtkErrorMacro("Bad render window");
    return;
    }

  this->MakeCurrent();

  // load required extensions
  vtkOpenGLClearErrorMacro();
  vtkOpenGLExtensionManager *extensions = openGLRenderWindow->GetExtensionManager();
  extensions->LoadExtension("GL_ARB_framebuffer_object");
  extensions->LoadExtension("GL_ARB_shader_objects");
  extensions->LoadExtension("GL_ARB_vertex_shader");
  extensions->LoadExtension("GL_ARB_fragment_shader");
  extensions->LoadExtension("GL_ARB_vertex_buffer_object");
  extensions->LoadExtension("GL_ARB_vertex_program");
  extensions->LoadExtension("GL_VERSION_1_2");
  extensions->LoadExtension("GL_ARB_multitexture");

  vtkOpenGLCheckErrorMacro("after extension load");

  // generate and bind our Framebuffer
  vtkgl::GenFramebuffers(1, &(this->FramebufferID));
  vtkgl::BindFramebuffer(vtkgl::FRAMEBUFFER, this->FramebufferID);
  vtkOpenGLCheckErrorMacro("after binding framebuffer");

  this->Initialized = true;
}


//-----------------------------------------------------------------------------
bool vtkOpenGLShaderComputation::AcquireResultRenderbuffer()
{
  //
  // adapted from
  // https://www.opengl.org/wiki/Framebuffer_Object_Examples
  //

  this->MakeCurrent();

  int resultDimensions[3];
  this->ResultImageData->GetDimensions(resultDimensions);

  vtkOpenGLClearErrorMacro();

  //
  // Create and attach a color buffer
  // * We must bind this->ColorRenderbufferID before we call glRenderbufferStorage
  // * The storage format is RGBA8
  // * Attach color buffer to FBO
  //
  vtkgl::GenRenderbuffers(1, &(this->ColorRenderbufferID));
  vtkgl::BindRenderbuffer(vtkgl::RENDERBUFFER, this->ColorRenderbufferID);
  vtkgl::RenderbufferStorage(vtkgl::RENDERBUFFER, GL_RGBA8,
                             resultDimensions[0], resultDimensions[1]);
  vtkgl::FramebufferRenderbuffer(vtkgl::FRAMEBUFFER,
                                 vtkgl::COLOR_ATTACHMENT0,
                                 vtkgl::RENDERBUFFER,
                                 this->ColorRenderbufferID);
  vtkOpenGLCheckErrorMacro("after binding color renderbuffer");

  //
  // Now do the same for the depth buffer
  //
  vtkgl::GenRenderbuffers(1, &(this->DepthRenderbufferID));
  vtkgl::BindRenderbuffer(vtkgl::RENDERBUFFER, this->DepthRenderbufferID);
  vtkgl::RenderbufferStorage(vtkgl::RENDERBUFFER, vtkgl::DEPTH_COMPONENT24,
                             resultDimensions[0], resultDimensions[1]);
  vtkgl::FramebufferRenderbuffer(vtkgl::FRAMEBUFFER,
                                 vtkgl::DEPTH_ATTACHMENT,
                                 vtkgl::RENDERBUFFER,
                                 this->DepthRenderbufferID);
  vtkOpenGLCheckErrorMacro("after binding depth renderbuffer");

  //
  // Does the GPU support current Framebuffer configuration?
  //
  GLenum status;
  status = vtkgl::CheckFramebufferStatus(vtkgl::FRAMEBUFFER);
  switch(status)
    {
    case vtkgl::FRAMEBUFFER_COMPLETE:
      break;
    default:
      vtkOpenGLCheckErrorMacro("after bad framebuffer status");
      vtkErrorMacro("Bad framebuffer configuration, status is: " << status);
      return false;
    }

  //
  // now we can render to the FBO (also called RenderBuffer)
  //
  vtkgl::BindFramebuffer(vtkgl::FRAMEBUFFER, this->FramebufferID);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  vtkOpenGLCheckErrorMacro("after clearing renderbuffers");

  //
  // Set up a normalized rendering environment
  //
  glViewport(0, 0, resultDimensions[0], resultDimensions[1]);
  vtkOpenGLCheckErrorMacro("after 1 normalizing environment");
  glMatrixMode(GL_PROJECTION);
  vtkOpenGLCheckErrorMacro("after 2 normalizing environment");
  glLoadIdentity();
  vtkOpenGLCheckErrorMacro("after 3 normalizing environment");
  glOrtho(0.0, resultDimensions[0], 0.0, resultDimensions[1], -1.0, 1.0);
  vtkOpenGLCheckErrorMacro("after 4 normalizing environment");
  glMatrixMode(GL_MODELVIEW);
  vtkOpenGLCheckErrorMacro("after 5 normalizing environment");
  glLoadIdentity();
  vtkOpenGLCheckErrorMacro("after 6 normalizing environment");
  glDisable(GL_BLEND);
  vtkOpenGLCheckErrorMacro("after 8 normalizing environment");
  glEnable(GL_DEPTH_TEST);
  vtkOpenGLCheckErrorMacro("after 9 normalizing environment");

  vtkOpenGLCheckErrorMacro("after framebuffer acquisition");
  return true;
}

//----------------------------------------------------------------------------
void vtkOpenGLShaderComputation::ReleaseResultRenderbuffer()
{

  this->MakeCurrent();
  vtkOpenGLClearErrorMacro();
  //Delete temp resources
  if (this->ColorRenderbufferID != 0)
    {
    vtkgl::DeleteRenderbuffers(1, &(this->ColorRenderbufferID));
    }
  if (this->DepthRenderbufferID != 0)
    {
    vtkgl::DeleteRenderbuffers(1, &(this->DepthRenderbufferID));
    }
  vtkOpenGLCheckErrorMacro("after framebuffer release");
}

//----------------------------------------------------------------------------
// Perform the computation
//
void vtkOpenGLShaderComputation::Compute(float slice)
{
  // bail out early if we aren't configured corretly
  if (this->VertexShaderSource == NULL || this->FragmentShaderSource == NULL)
    {
    vtkErrorMacro("Both vertex and fragment shaders are needed for a shader computation.");
    return;
    }

  // ensure that all our OpenGL calls go to the correct context
  this->MakeCurrent();

  //
  // Does the GPU support current Framebuffer configuration?
  //
  GLenum status;
  status = vtkgl::CheckFramebufferStatus(vtkgl::FRAMEBUFFER);
  switch(status)
    {
    case vtkgl::FRAMEBUFFER_COMPLETE:
      break;
    default:
      vtkErrorMacro("Can't compute in incompete framebuffer; status is: " << status);
      return;
    }

  // Configure the program and the input data
  if (!this->UpdateProgram())
    {
    vtkErrorMacro("Could not update shader program.");
    return;
    }

  // define a normalized computing surface
  GLfloat planeVertices[] = { -1.0f, -1.0f, 0.0f,
                              -1.0f,  1.0f, 0.0f,
                               1.0f,  1.0f, 0.0f,
                               1.0f, -1.0f, 0.0f
                        };
  GLuint planeVerticesSize = sizeof(GLfloat)*3*4;
  GLfloat planeTextureCoordinates[] = { 0.0f, 0.0f,
                                        0.0f, 1.0f,
                                        1.0f, 1.0f,
                                        1.0f, 0.0f
                        };
  GLuint planeTextureCoordinatesSize = sizeof(GLfloat)*2*4;

  vtkOpenGLClearErrorMacro();
  // Use the program object
  vtkgl::UseProgramObjectARB ( this->ProgramObject );
  vtkOpenGLCheckErrorMacro("after use program");

  // put vertices in a buffer and make it available to the program
  GLuint vertexLocation = vtkgl::GetAttribLocationARB(this->ProgramObject, "vertexAttribute");
  GLuint planeVerticesBuffer;
  vtkgl::GenBuffersARB(1, &planeVerticesBuffer);
  vtkgl::BindBufferARB(vtkgl::ARRAY_BUFFER, planeVerticesBuffer);
  vtkgl::BufferDataARB(vtkgl::ARRAY_BUFFER, planeVerticesSize, planeVertices, vtkgl::STATIC_DRAW);
  vtkgl::EnableVertexAttribArrayARB ( vertexLocation );
  vtkgl::VertexAttribPointerARB ( vertexLocation, 3, GL_FLOAT, GL_FALSE, 0, 0 );
  vtkOpenGLCheckErrorMacro("after vertices");

  // texture coordinates in a buffer
  GLuint textureCoordinatesLocation = vtkgl::GetAttribLocationARB(this->ProgramObject,
                                                          "textureCoordinateAttribute");
  GLuint textureCoordinatesBuffer;
  vtkgl::GenBuffersARB(1, &textureCoordinatesBuffer);
  vtkgl::BindBufferARB(vtkgl::ARRAY_BUFFER, textureCoordinatesBuffer);
  vtkgl::BufferDataARB(vtkgl::ARRAY_BUFFER, planeTextureCoordinatesSize, planeTextureCoordinates, vtkgl::STATIC_DRAW);
  vtkgl::EnableVertexAttribArrayARB ( textureCoordinatesLocation );
  vtkgl::VertexAttribPointerARB ( textureCoordinatesLocation, 2, GL_FLOAT, GL_FALSE, 0, 0 );
  vtkOpenGLCheckErrorMacro("after texture coordinates");

  // Iterate through all standard texture units and if one of them
  // is used as a uniform variable in the program, set the corresponding value.
  // This relies on vtkOpenGLTextureImage (or something else) to have
  // set up the texture units with data.
  // Up to 48 units are meant to be supported on any OpenGL implementation
  // but the defined enums appear to only go to 32.
  #define __TEXTURE_UNIT_COUNT 16 // TODO: maybe expose parameter of how many textures to look for
  char textureUnitUniformString[14]; // 14 length of "textureUnit__" including \0
  strncpy(textureUnitUniformString, "textureUnit__", 14);
  char textureUnitLength = 11; // Up to the two underscores that will be replaced
  char asciiUnit[3]; // target for snprintf
  int unitIndex;
  for (unitIndex = 0; unitIndex < __TEXTURE_UNIT_COUNT; unitIndex++)
    {
    snprintf(asciiUnit, 3, "%d", unitIndex);
    strncpy(textureUnitUniformString + textureUnitLength, asciiUnit, 2);
    GLint textureUnitSamplerLocation = vtkgl::GetUniformLocationARB(this->ProgramObject, textureUnitUniformString);
    if ( textureUnitSamplerLocation >= 0 )
      {
      vtkgl::Uniform1iARB(textureUnitSamplerLocation, unitIndex);
      vtkOpenGLCheckErrorMacro("after setting texture unit uniform " << unitIndex);
      }
    }
  vtkOpenGLCheckErrorMacro("after setting texture unit uniforms");

  // pass in the slice location.
  // TODO: generalize uniform arguments, create vtkVariantMap
  GLint sliceLocation = vtkgl::GetUniformLocationARB(this->ProgramObject, "slice");
  if ( sliceLocation >= 0 )
    {
    vtkgl::Uniform1fARB(sliceLocation, slice);
    }

  //
  // GO!
  //
  glDrawArrays ( GL_QUADS, 0, 4 );

  vtkOpenGLCheckErrorMacro("after drawing");

  //
  // Don't use the program or the framebuffer anymore
  //
  vtkgl::UseProgramObjectARB ( 0 );
}

//----------------------------------------------------------------------------
void vtkOpenGLShaderComputation::ReadResult()
{

  this->MakeCurrent();
  vtkOpenGLClearErrorMacro();
  // check and set up the result area
  if (this->ResultImageData == NULL
      ||
      this->ResultImageData->GetPointData() == NULL
      ||
      this->ResultImageData->GetPointData()->GetScalars() == NULL
      ||
      this->ResultImageData->GetPointData()->GetScalars()->GetVoidPointer(0) == NULL)
    {
    vtkErrorMacro("Result image data is not correctly set up.");
    return;
    }
  int resultDimensions[3];
  this->ResultImageData->GetDimensions(resultDimensions);
  vtkPointData *pointData = this->ResultImageData->GetPointData();
  vtkDataArray *scalars = pointData->GetScalars();
  void *resultPixels = scalars->GetVoidPointer(0);

  //
  // Collect the results of the calculation back into the image data
  //
  int componentCount = this->ResultImageData->GetNumberOfScalarComponents();
  GLuint format;
  if ( componentCount == 1 )
    {
    format = GL_LUMINANCE;
    }
  else if ( componentCount == 4 )
    {
    format = GL_RGBA;
    }
  else
    {
    vtkErrorMacro("Must have 1 or 4 component image data for texture");
    return;
    }

  GLuint scalarType = vtkOpenGLTextureImage::vtkScalarTypeToGLType(this->ResultImageData->GetScalarType());
  glReadPixels(0, 0, resultDimensions[0], resultDimensions[1], format, scalarType, resultPixels);
  pointData->Modified();

  vtkOpenGLCheckErrorMacro("after reading back");
}

//----------------------------------------------------------------------------
void vtkOpenGLShaderComputation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  os << indent << "Initialized: " << this->Initialized << "\n";
  if ( this->VertexShaderSource )
    {
    os << indent << "VertexShaderSource: " << this->VertexShaderSource << "\n";
    }
  else
    {
    os << indent << "VertexShaderSource: (none)\n";
    }
  if ( this->FragmentShaderSource )
    {
    os << indent << "FragmentShaderSource: " << this->FragmentShaderSource << "\n";
    }
  else
    {
    os << indent << "FragmentShaderSource: (none)\n";
    }
  if ( this->ResultImageData )
    {
    os << indent << "ResultImageData: " << this->ResultImageData << "\n";
    }
  else
    {
    os << indent << "ResultImageData: (none)\n";
    }
  os << indent << "ProgramObject: " << this->ProgramObject << "\n";
  os << indent << "ProgramObjectMTime: " << this->ProgramObjectMTime << "\n";
  os << indent << "FramebufferID: " << this->FramebufferID << "\n";
  os << indent << "ColorRenderbufferID: " << this->ColorRenderbufferID << "\n";
  os << indent << "DepthRenderbufferID: " << this->DepthRenderbufferID << "\n";
}
