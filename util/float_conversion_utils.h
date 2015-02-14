/*
 * float_conversion_utils.h
 *
 *  Created on: 7 Feb 2015
 *      Author: mpogliani
 */

#ifndef FLOAT_CONVERSION_UTILS_H_
#define FLOAT_CONVERSION_UTILS_H_

extern "C" {

	int64_t fixpoint_convert_double(double val, uint64_t dbw);

	int64_t fixpoint_convert_single(float val, uint64_t dbw);

}

#endif /* FLOAT_CONVERSION_UTILS_H_ */
