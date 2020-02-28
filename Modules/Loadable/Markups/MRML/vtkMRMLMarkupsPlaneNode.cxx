/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

// MRML includes
#include "vtkMRMLMarkupsDisplayNode.h"
#include "vtkMRMLMarkupsPlaneNode.h"
#include "vtkMRMLMarkupsFiducialStorageNode.h"
#include "vtkMRMLScene.h"

// VTK includes
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkTriangle.h>

// STD includes
#include <sstream>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLMarkupsPlaneNode);


//----------------------------------------------------------------------------
vtkMRMLMarkupsPlaneNode::vtkMRMLMarkupsPlaneNode()
{
  this->MaximumNumberOfControlPoints = 3;
  this->RequiredNumberOfControlPoints = 3;
  this->SizeMode = SizeModeAuto;
  this->AutoSizeScaling = 1.0;
}

//----------------------------------------------------------------------------
vtkMRMLMarkupsPlaneNode::~vtkMRMLMarkupsPlaneNode()
= default;

//----------------------------------------------------------------------------
void vtkMRMLMarkupsPlaneNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of,nIndent);
}

//----------------------------------------------------------------------------
void vtkMRMLMarkupsPlaneNode::ReadXMLAttributes(const char** atts)
{
  Superclass::ReadXMLAttributes(atts);
}

//----------------------------------------------------------------------------
void vtkMRMLMarkupsPlaneNode::Copy(vtkMRMLNode *anode)
{
  Superclass::Copy(anode);
}

//----------------------------------------------------------------------------
void vtkMRMLMarkupsPlaneNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
void vtkMRMLMarkupsPlaneNode::GetNormal(double normal[3])
{
  if (!normal)
    {
    return;
    }

  if (this->GetNumberOfControlPoints() < 3)
    {
    return;
    }

  double x[3] = { 0 };
  double y[3] = { 0 };
  this->GetVectors(x, y, normal);
}

//----------------------------------------------------------------------------
void vtkMRMLMarkupsPlaneNode::GetOrigin(double origin[3])
{
  if (!origin)
    {
    return;
    }

  if (this->GetNumberOfControlPoints() < 1)
    {
    return;
    }
  this->GetNthControlPointPositionWorld(0, origin);
}

//----------------------------------------------------------------------------
void vtkMRMLMarkupsPlaneNode::GetVectors(double x[3], double y[3], double z[3])
{
  if (!x || !y || !z)
    {
    return;
    }

  if (this->GetNumberOfControlPoints() < 3)
    {
    return;
    }

  double origin[3] = { 0 };
  this->GetNthControlPointPositionWorld(0, origin);
  double point1[3] = { 0 };
  this->GetNthControlPointPositionWorld(1, point1);
  double point2[3] = { 0 };
  this->GetNthControlPointPositionWorld(2, point2);

  vtkMath::Subtract(point1, origin, x);
  vtkMath::Normalize(x);

  double tempVector[3] = { 0 };
  vtkMath::Subtract(point2, origin, tempVector);
  vtkMath::Cross(x, tempVector, z);
  vtkMath::Normalize(z);

  vtkMath::Cross(z, x, y);
  vtkMath::Normalize(z);
}

//----------------------------------------------------------------------------
void vtkMRMLMarkupsPlaneNode::GetSize(double size[3])
{
  if (this->GetNumberOfControlPoints() < 3)
    {
    return;
    }

  // Size mode auto means we need to recalculate the diameter of the plane from the control points.
  if (this->SizeMode == vtkMRMLMarkupsPlaneNode::SizeModeAuto)
    {
    double origin[3] = { 0.0 };
    double point1[3] = { 0.0 };
    double point2[3] = { 0.0 };
    this->GetNthControlPointPositionWorld(0, origin);
    this->GetNthControlPointPositionWorld(1, point1);
    this->GetNthControlPointPositionWorld(2, point2);

    double x[3], y[3], z[3] = { 0 };
    this->GetVectors(x, y, z);

    // Update the plane
    double vector1[3] = { 0 };
    vtkMath::Subtract(point1, origin, vector1);

    double vector2[3] = { 0 };
    vtkMath::Subtract(point2, origin, vector2);

    double point1X = std::abs(vtkMath::Dot(vector1, x));
    double point2X = std::abs(vtkMath::Dot(vector2, x));
    double xMax = std::max({ 0.0, point1X, point2X });

    double point1Y = std::abs(vtkMath::Dot(vector1, y));
    double point2Y = std::abs(vtkMath::Dot(vector2, y));
    double yMax = std::max({ 0.0, point1Y, point2Y });

    this->Size[0] = 2 * xMax * this->AutoSizeScaling;
    this->Size[1] = 2 * yMax * this->AutoSizeScaling;
    this->Size[2] = 0.0;
    }

  for (int i = 0; i < 3; ++i)
    {
    size[i] = this->Size[i];
    }
}
