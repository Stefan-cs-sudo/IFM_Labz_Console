#include "StringUtils.h"
#include "stdint.h"

/*!
 * \brief function used to copy a string from a source address to a destination address.
 * \param src is the reference to the source string.
 * \param dest is the reference to the destination string.
 * \param MaxLen is the maximum length to write. 
 * \note if the source string is longer than MaxLen, 
         then the extra characters will be skipped. 
 * \note if the source string is shorter than MaxLen, then the destination string
 *       will be padded with '\0' character on the empty remaining positions. */
void StringUtils_stringCopy(const char* src, char* dest, uint8_t MaxLen)
{
  uint8_t CharIdx = 0;
  while( (src[CharIdx] != '\0') && (CharIdx < MaxLen) ) 
  {
    dest[CharIdx] = src[CharIdx];
    CharIdx++;
  }
  if(MaxLen == CharIdx)
  {
    dest[MaxLen-1] = '\0';
  }
  else
  {
    uint8_t PaddingIdx = CharIdx; 
    for(PaddingIdx = CharIdx; PaddingIdx<MaxLen; PaddingIdx++)
    {
      dest[PaddingIdx] = '\0';
    }
  }
}
