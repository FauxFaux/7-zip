// SysIconUtils.h

#pragma once

#ifndef __SYSICONUTILS_H
#define __SYSICONUTILS_H

#include "Common/String.h"

struct CExtIconPair
{
  CSysString Ext;
  int IconIndex;
  CSysString TypeName;

};

inline bool operator==(const CExtIconPair &a1, const CExtIconPair &a2)
{
  return (a1.Ext == a2.Ext);
}

inline bool operator<(const CExtIconPair &a1, const CExtIconPair &a2)
{
  return (a1.Ext < a2.Ext);
}

class CExtToIconMap
{
  int _dirIconIndex;
  CSysString _dirTypeName;
  int _noExtIconIndex;
  CSysString _noExtTypeName;
  CObjectVector<CExtIconPair> _map;
public:
  CExtToIconMap(): _dirIconIndex(-1), _noExtIconIndex(-1) {}
  void Clear() 
  {
    _dirIconIndex = -1;
    _noExtIconIndex = -1;
    _map.Clear();
  }
  int GetIconIndex(UINT32 attributes, const CSysString &fileName, 
      CSysString &typeName);
  int GetIconIndex(UINT32 attributes, const CSysString &fileName);
};

DWORD_PTR GetRealIconIndex(LPCTSTR path, UINT32 attributes, int &iconIndex);
#ifndef _UNICODE
// DWORD_PTR GetRealIconIndex(LPCWSTR path, UINT32 attributes, int &iconIndex);
#endif
DWORD_PTR GetRealIconIndex(const CSysString &fileName, UINT32 attributes, int &iconIndex, CSysString &typeName);
int GetIconIndexForCSIDL(int aCSIDL);


#endif

