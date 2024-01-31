/**
 * @brief Michael Ashikhmin tone mapping operator
 *
 * This file is a part of LuminanceHDR package, based on pfstmo.
 * ----------------------------------------------------------------------
 * Copyright (C) 2003,2004 Grzegorz Krawczyk
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * ----------------------------------------------------------------------
 *
 * @author Akiko Yoshida, <yoshida@mpi-sb.mpg.de>
 * @author Grzegorz Krawczyk, <krawczyk@mpi-sb.mpg.de>
 *
 * $Id: tmo_ashikhmin02.h,v 1.4 2004/11/16 13:40:46 yoshida Exp $
 */

#ifndef TMO_ASHIKHMIN02_H
#define TMO_ASHIKHMIN02_H

#include <Libpfs/array2d_fwd.h>

namespace pfs {
class Frame;
class Progress;
}

//! \brief Michael Ashikhmin tone mapping operator
//!
//! \param Y [in] image luminance values
//! \param L [out] tone mapped values
//! \param maxLum maximum luminance in the image
//! \param avLum logarithmic average of luminance in the image
//! \param simple_flag true: use only tone mapping function (global version of
//! the operator)
//! \param lc_value local contrast threshold
//! \param eq chose equation number from the paper (ie equation 2. or 4. )
//!
int tmo_ashikhmin02(pfs::Array2Df *Y, pfs::Array2Df *L, float maxLum,
                    float minLum, float avLum, bool simple_flag, float lc_value,
                    int eq, pfs::Progress &ph);

#endif  // TMO_ASHIKHMIN02_H
