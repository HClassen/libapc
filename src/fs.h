#ifndef FS_HEADER
#define FS_HEADER

#include "apc.h"

/* closes an apc_file handle
 * @param apf_file *file
 * @return void 
 */
void file_close(apc_file *file);

#endif