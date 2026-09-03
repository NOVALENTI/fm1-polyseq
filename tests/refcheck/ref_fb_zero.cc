/* Test-only accessor implementation. The private->public redefinition is
 * quarantined to this TU so the main test driver keeps normal access. */

#define private public
#include "msfa_orig/dx7note.h"
#undef private

void RefZeroFb(Dx7Note *note)
{
    note->fb_buf_[0] = 0;
    note->fb_buf_[1] = 0;
}
