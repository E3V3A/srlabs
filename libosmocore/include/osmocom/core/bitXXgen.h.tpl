/*
 * bitXXgen.h
 *
 * Copyright (C) 2014  Max <max.suraev@fairwaves.co>
 *
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

/*! \brief load unaligned n-byte integer (little-endian encoding) into uintXX_t
 *  \param[in] p Buffer where integer is stored
 *  \param[in] n Number of bytes stored in p
 *  \returns XX bit unsigned integer
 */
static inline uintXX_t osmo_loadXXle_ext(const void *p, uint8_t n)
{
	uint8_t i;
	uintXX_t r = 0;
	const uint8_t *q = (uint8_t *)p;
	for(i = 0; i < n; r |= ((uintXX_t)q[i] << (8 * i)), i++);
	return r;
}

/*! \brief load unaligned n-byte integer (big-endian encoding) into uintXX_t
 *  \param[in] p Buffer where integer is stored
 *  \param[in] n Number of bytes stored in p
 *  \returns XX bit unsigned integer
 */
static inline uintXX_t osmo_loadXXbe_ext(const void *p, uint8_t n)
{
	uint8_t i;
	uintXX_t r = 0;
	const uint8_t *q = (uint8_t *)p;
	for(i = 0; i < n; r |= ((uintXX_t)q[i] << (XX - 8* (1 + i))), i++);
	return r;
}


/*! \brief store unaligned n-byte integer (little-endian encoding) into uintXX_t
 *  \param[in] x unsigned XX bit integer
 *  \param[out] p Buffer to store integer
 *  \param[in] n Number of bytes to store
 */
static inline void osmo_storeXXle_ext(uintXX_t x, void *p, uint8_t n)
{
	uint8_t i;
	uint8_t *q = (uint8_t *)p;
	for(i = 0; i < n; q[i] = (x >> i * 8) & 0xFF, i++);
}

/*! \brief store unaligned n-byte integer (big-endian encoding) into uintXX_t
 *  \param[in] x unsigned XX bit integer
 *  \param[out] p Buffer to store integer
 *  \param[in] n Number of bytes to store
 */
static inline void osmo_storeXXbe_ext(uintXX_t x, void *p, uint8_t n)
{
	uint8_t i;
	uint8_t *q = (uint8_t *)p;
	for(i = 0; i < n; q[i] = (x >> ((n - 1 - i) * 8)) & 0xFF, i++);
}


/* Convenience function for most-used cases */


/*! \brief load unaligned XX-bit integer (little-endian encoding) */
static inline uintXX_t osmo_loadXXle(const void *p)
{
	return osmo_loadXXle_ext(p, XX / 8);
}

/*! \brief load unaligned XX-bit integer (big-endian encoding) */
static inline uintXX_t osmo_loadXXbe(const void *p)
{
	return osmo_loadXXbe_ext(p, XX / 8);
}


/*! \brief store unaligned XX-bit integer (little-endian encoding) */
static inline void osmo_storeXXle(uintXX_t x, void *p)
{
	osmo_storeXXle_ext(x, p, XX / 8);
}

/*! \brief store unaligned XX-bit integer (big-endian encoding) */
static inline void osmo_storeXXbe(uintXX_t x, void *p)
{
	osmo_storeXXbe_ext(x, p, XX / 8);
}
