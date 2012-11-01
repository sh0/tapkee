#ifndef libedrt_methods_h_
#define libedrt_methods_h_

#include "../defines.hpp"
#include <iostream>
#include <shogun/lib/SGMatrix.h>

using std::cout;
using std::endl;

template <class RandomAccessIterator, class PairwiseCallback>
WeightMatrix kltsa_weight_matrix(RandomAccessIterator begin, RandomAccessIterator end, 
                                Neighbors neighbors, PairwiseCallback callback, unsigned int target_dimension)
{
	typedef Eigen::VectorXd DenseVector;
	typedef Eigen::MatrixXd DenseMatrix;
	typedef Eigen::Triplet<double> SparseTriplet;
	typedef std::vector<SparseTriplet> SparseTriplets;
	
	int k = neighbors[0].size();

	SparseTriplets sparse_triplets;
	sparse_triplets.reserve(k*k*(end-begin));

	RandomAccessIterator iter;
	RandomAccessIterator iter_begin = begin, iter_end = end;
	DenseMatrix gram_matrix = DenseMatrix::Zero(k,k);
	DenseVector col_means(k), row_means(k);
	DenseVector rhs = DenseVector::Ones(k);
	WeightMatrix weight_matrix(end-begin,end-begin);
	for (RandomAccessIterator iter=iter_begin; iter!=iter_end; ++iter)
	{
		//cout << "KV = " << kernel_value << endl;
		const LocalNeighbors& current_neighbors = neighbors[iter-begin];
	
		for (int i=0; i<k; ++i)
		{
			//cout << "N " << current_neighbors[i] << " " << callback(*iter,begin[current_neighbors[i]]);
			for (int j=0; j<k; ++j)
				gram_matrix(i,j) = callback(begin[current_neighbors[i]],begin[current_neighbors[j]]);
		}
		//cout << endl;
		//cout << "Gram" << endl;
		//cout << gram_matrix << endl;

		//shogun::SGMatrix<float64_t>::center_matrix(gram_matrix.data(),k,k);
		for (int i=0; i<k; ++i)
		{
			col_means[i] = gram_matrix.col(i).mean();
			row_means[i] = gram_matrix.row(i).mean();
		}
		double grand_mean = gram_matrix.mean();
		gram_matrix.array() += grand_mean;
		gram_matrix.rowwise() -= col_means.transpose();
		gram_matrix.colwise() -= row_means;
		
		//cout << "Gram centered" << endl;
		//cout << gram_matrix << endl;
		Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> sae_solver;
		sae_solver.compute(gram_matrix);

		//cout << "All eigenvectors" << endl;
		//cout << sae_solver.eigenvectors();
	
		DenseMatrix G = DenseMatrix::Zero(k,target_dimension+1);
		G.col(0).setConstant(1/sqrt(k));

		for (int i=0; i<target_dimension; i++)
			G.col(i+1) += sae_solver.eigenvectors().col(k-i-1);
		//cout << "G" << endl;
		//cout << G << endl;
		gram_matrix = G * G.transpose();
		//cout << "gigit" << endl;
		//cout << gram_matrix << endl;
		//cout << "Eigenvalues = [";
		//for (int i=0; i<target_dimension; ++i)
		//	cout << sae_solver.eigenvalues()[k-target_dimension+i] << ", ";
		//cout << " ]" << endl;
		sparse_triplets.push_back(SparseTriplet(iter-begin,iter-begin,1e-8));
		for (int i=0; i<k; ++i)
		{
			sparse_triplets.push_back(SparseTriplet(current_neighbors[i],current_neighbors[i],1.0));
			for (int j=0; j<k; ++j)
				sparse_triplets.push_back(SparseTriplet(current_neighbors[i],current_neighbors[j],
				                                        -gram_matrix(i,j)));
		}
	}

	weight_matrix.setFromTriplets(sparse_triplets.begin(),sparse_triplets.end());
	cout << weight_matrix.cols() << endl;
	cout << weight_matrix.rows() << endl;
	return weight_matrix;
}
template <class RandomAccessIterator, class PairwiseCallback>
WeightMatrix klle_weight_matrix(RandomAccessIterator begin, RandomAccessIterator end, 
                                Neighbors neighbors, PairwiseCallback callback)
{
	typedef Eigen::VectorXd DenseVector;
	typedef Eigen::MatrixXd DenseMatrix;
	typedef Eigen::Triplet<double> SparseTriplet;
	typedef std::vector<SparseTriplet> SparseTriplets;
	
	int k = neighbors[0].size();

	SparseTriplets sparse_triplets;
	sparse_triplets.reserve(k*k*(end-begin));

	RandomAccessIterator iter;
	RandomAccessIterator iter_begin = begin, iter_end = end;
	DenseMatrix gram_matrix = DenseMatrix::Zero(k,k);
	DenseVector dots(k);
	DenseVector rhs = DenseVector::Ones(k);
	DenseVector weights;
	for (RandomAccessIterator iter=iter_begin; iter!=iter_end; ++iter)
	{
		double kernel_value = callback(*iter,*iter);
		//cout << "KV = " << kernel_value << endl;
		const LocalNeighbors& current_neighbors = neighbors[iter-begin];
		
		for (int i=0; i<k; ++i)
			dots[i] = callback(*iter, begin[current_neighbors[i]]);
		//cout << "Dots " << dots << endl;

		for (int i=0; i<k; ++i)
		{
			for (int j=0; j<k; ++j)
				gram_matrix(i,j) = kernel_value - dots(i) - dots(j) + callback(begin[current_neighbors[i]],begin[current_neighbors[j]]);
		}
		//cout << "G = [" << gram_matrix << "]" << endl;
		double trace = gram_matrix.trace();
		gram_matrix.diagonal().array() += 1e-3*trace;
		weights = gram_matrix.ldlt().solve(rhs);
		weights /= weights.sum();

		//cout << "Neighbors = [";
		//for (int i=0; i<k; ++i)
		//	cout << current_neighbors[i] << ", ";
		//cout << " ]" << endl;
		//cout << "Weights = [" << weights << "]" << endl; 
		sparse_triplets.push_back(SparseTriplet(iter-begin,iter-begin,1.0));
		for (int i=0; i<k; ++i)
		{
			sparse_triplets.push_back(SparseTriplet(current_neighbors[i],iter-begin,
			                                        -weights[i]));
			sparse_triplets.push_back(SparseTriplet(iter-begin,current_neighbors[i],
			                                        -weights[i]));
			for (int j=0; j<k; ++j)
				sparse_triplets.push_back(SparseTriplet(current_neighbors[i],current_neighbors[j],
				                                        +weights(i)*weights(j)));
		}
	}

	WeightMatrix weight_matrix(end-begin,end-begin);
	weight_matrix.setFromTriplets(sparse_triplets.begin(),sparse_triplets.end());

	return weight_matrix;
}

#endif