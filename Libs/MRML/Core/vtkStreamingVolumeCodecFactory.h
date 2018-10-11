/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkStreamingVolumeCodecFactory.h,v $
  Date:      $Date: 2006/03/19 17:12:29 $
  Version:   $Revision: 1.13 $

=========================================================================auto=*/


#ifndef __vtkStreamingVolumeCodecFactory_h
#define __vtkStreamingVolumeCodecFactory_h

// VTK includes
#include <vtkObject.h>
#include <vtkSmartPointer.h>

// STD includes
#include <map>

// MRML includes
#include "vtkMRML.h"
#include "vtkStreamingVolumeCodec.h"

/// \ingroup Volumes
/// \brief Class that can create compresion device for streaming volume instances.
///
/// This singleton class is a repository of all compression codecs for compressing volume .
/// Singleton pattern adopted from vtkEventBroker class.
class VTK_MRML_EXPORT vtkStreamingVolumeCodecFactory : public vtkObject
{
public:

  vtkTypeMacro(vtkStreamingVolumeCodecFactory, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  /// Registers a new video compression codec
  /// \param newCodecPointer Function pointer to the codec type new function (e.g. (PointerToCodecBaseNew)&vtkStreamingVolumeCodec::New )
  bool RegisterStreamingCodec(vtkSmartPointer<vtkStreamingVolumeCodec> codec);

  /// Removes a codec from the factory
  /// This does not affect codecs that have already been instantiated
  /// \param codecClassName full name of the codec class that is being unregistered
  /// Returns true if the codec is successfully unregistered
  bool UnregisterStreamingCodecByClassName(const std::string& codecClassName);

  /// Get pointer to codec new function, or NULL if the codec type not registered
  /// Usage: vtkSmartPointer<vtkStreamingVolumeCodec> codec = GetVolumeCodecNewPointerByType("igtlioVideoDevice")();
  /// Returns NULL if no matching codec can be found
  vtkStreamingVolumeCodec* CreateCodecByClassName(const std::string& codecClassName);

  /// Creates a new codec that matches the specified FourCC code
  /// \param codecFourCC FourCC representing the encoding method
  /// See https://www.fourcc.org/codecs.php for an incomplete list
  /// Returns NULL if no matching codec can be found
  vtkStreamingVolumeCodec* CreateCodecByFourCC(const std::string codecFourCC);

  /// Returns a list of all registered Codecs
  const std::vector<vtkSmartPointer<vtkStreamingVolumeCodec> >& GetStreamingCodecClasses();

  /// Get FourCCs for all registered Codecs
  std::vector<std::string> GetStreamingCodecFourCCs();

public:
  /// Return the singleton instance with no reference counting.
  static vtkStreamingVolumeCodecFactory* GetInstance();

  /// This is a singleton pattern New.  There will only be ONE
  /// reference to a vtkStreamingVolumeCodecFactory object per process.  Clients that
  /// call this must call Delete on the object so that the reference
  /// counting will work. The single instance will be unreferenced when
  /// the program exits.
  static vtkStreamingVolumeCodecFactory* New();

protected:
  vtkStreamingVolumeCodecFactory();
  ~vtkStreamingVolumeCodecFactory();
  vtkStreamingVolumeCodecFactory(const vtkStreamingVolumeCodecFactory&);
  void operator=(const vtkStreamingVolumeCodecFactory&);

  friend class vtkStreamingVolumeCodecFactoryInitialize;
  typedef vtkStreamingVolumeCodecFactory Self;

  // Singleton management functions.
  static void classInitialize();
  static void classFinalize();

  /// Registered converter rules
  /*! Map codec types and the New() static methods of vtkStreamingVolumeCodec classes */
  std::vector< vtkSmartPointer<vtkStreamingVolumeCodec> > RegisteredCodecs;
};


/// Utility class to make sure qSlicerModuleManager is initialized before it is used.
class VTK_MRML_EXPORT vtkStreamingVolumeCodecFactoryInitialize
{
public:
  typedef vtkStreamingVolumeCodecFactoryInitialize Self;

  vtkStreamingVolumeCodecFactoryInitialize();
  ~vtkStreamingVolumeCodecFactoryInitialize();
private:
  static unsigned int Count;
};

/// This instance will show up in any translation unit that uses
/// vtkStreamingVolumeCodecFactory.  It will make sure vtkStreamingVolumeCodecFactory is initialized
/// before it is used.
static vtkStreamingVolumeCodecFactoryInitialize vtkStreamingVolumeCodecFactoryInitializer;

#endif
