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
