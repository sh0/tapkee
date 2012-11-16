/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Written (w) 2012, Fernando J. Iglesias Garcia
 * Copyright (c) 2012, Fernando J. Iglesias Garcia
 */

#ifndef TAPKEE_SPE_H_
#define TAPKEE_SPE_H_

#include <algorithm>
#include <ctime>

#include "../defines.hpp"
#include "../utils/time.hpp"

template <class RandomAccessIterator, class PairwiseCallback>
EmbeddingResult spe_embedding(RandomAccessIterator begin, RandomAccessIterator end,
		PairwiseCallback callback, unsigned int k, unsigned int target_dimension,
		bool global_strategy, DefaultScalarType tolerance, int nupdates)
{
	timed_context context("SPE embedding computation");

	// The number of data points
	int N = end-begin;
	while (nupdates > N/2)
		nupdates = N/2;

	// Look for the maximum distance
	DefaultScalarType max = 0.0;
	for (RandomAccessIterator i_iter=begin; i_iter!=end; ++i_iter)
	{
		for (RandomAccessIterator j_iter=i_iter+1; j_iter!=end; ++j_iter)
		{
			max = std::max(max, callback(*i_iter,*j_iter));
		}
	}

	// Distances normalizer used in global strategy
	DefaultScalarType alpha = 0.0;
	if (global_strategy)
		alpha = 1.0 / max * std::sqrt(2.0);

	// Random embedding initialization, Y is the short for embedding_feature_matrix
	std::srand(std::time(0));
	DenseMatrix Y = (DenseMatrix::Random(target_dimension,N)
		       + DenseMatrix::Ones(target_dimension,N)) / 2;
	// Auxiliary diffference embedding feature matrix
	DenseMatrix Yd(target_dimension,nupdates);

	// SPE's main loop
	
	typedef std::vector<int> Indices;
	typedef std::vector<int>::iterator IndexIterator;

	// Maximum number of iterations
	int max_iter = 2000 + round(0.04 * N*N);
	if (!global_strategy)
		max_iter *= 3;
	// Learning parameter
	DefaultScalarType lambda = 1.0;
	// Vector of indices used for shuffling
	Indices indices(N);
	for (int i=0; i<N; ++i)
		indices[i] = i;
	// Vector with distances in the original space of the points to update
	DenseVector Rt(nupdates);
	DenseVector scale(nupdates);
	DenseVector D(nupdates);
	// Pointers to the indices of the elements to update
	IndexIterator ind1;
	IndexIterator ind2;

	for (int i=0; i<max_iter; ++i)
	{
		// Shuffle to select the vectors to update in this iteration
		std::random_shuffle(indices.begin(),indices.end());

		ind1 = indices.begin();
		ind2 = indices.begin()+nupdates;

		// Compute distances between the selected points in the embedded space
		for(int j=0; j<nupdates; ++j)
		{
			//FIXME it seems that here Euclidean distance is forced
			D[j] = (Y.col(*ind1) - Y.col(*ind2)).norm();
			++ind1, ++ind2;
		}

		// Get the corresponding distances in the original space
		if (global_strategy)
			Rt.fill(alpha);
		else // local_strategy
			Rt.fill(1);

		ind1 = indices.begin();
		ind2 = indices.begin()+nupdates;
		for (int j=0; j<nupdates; ++j)
			Rt[j] *= callback(*(begin + *ind1++), *(begin + *ind2++));

		// Compute some terms for update

		// Scale factor
		D.array() += tolerance;
		scale = (Rt-D).cwiseQuotient(D);

		ind1 = indices.begin();
		ind2 = indices.begin()+nupdates;
		// Difference matrix
		for (int j=0; j<nupdates; ++j)
		{
			Yd.col(j).noalias() = Y.col(*ind1) - Y.col(*ind2);

			++ind1, ++ind2;
		}

		ind1 = indices.begin();
		ind2 = indices.begin()+nupdates;
		// Update the location of the vectors in the embedded space
		for (int j=0; j<nupdates; ++j)
		{
			Y.col(*ind1) += lambda / 2 * scale[j] * Yd.col(j);
			Y.col(*ind2) -= lambda / 2 * scale[j] * Yd.col(j);

			++ind1, ++ind2;
		}

		// Update the learning parameter
		lambda = lambda - ( lambda / max_iter );
	}

	return EmbeddingResult(Y.transpose(),DenseVector());
};

#endif /* TAPKEE_SPE_H_ */
