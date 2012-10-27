#include "libedrt_methods.hpp"
#include <pthread.h>
#include <vector>
#include <map>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX(x,y) x>y ? x : y

double* klle_weight_matrix(
		int* neighborhood_matrix,
		int N,
		int k,
		int matrix_k,
		int num_threads,
		double reconstruction_shift,
		double (*kernel)(int, int, const void*),
		const void* user_data)
{
	int t;
	// allocate threads
	pthread_t* threads;// = SG_MALLOC(pthread_t, num_threads);
	thread_parameters_t* parameters = 
		new thread_parameters_t[num_threads];
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	//pthread_mutex_t W_matrix_lock;
	//PTHREAD_LOCK_INIT(&W_matrix_lock);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	// init storages to be used
	double* weight_matrix;// = SG_CALLOC(double, N*N);

	for (t=0; t<num_threads; t++)
	{
		parameters[t].thread = t;
		parameters[t].num_threads = num_threads;
		parameters[t].k = k;
		parameters[t].N = N;
		parameters[t].neighborhood_matrix = neighborhood_matrix;
		parameters[t].kernel = kernel;
		parameters[t].W_matrix = weight_matrix;
		parameters[t].user_data = user_data;
#ifdef HAVE_PTHREAD
		parameters[t].pthread_lock = &W_matrix_lock;
		pthread_create(&threads[t], &attr, klle_weight_matrix_thread, (void*)&parameters[t]);
	}
	for (t=0; t<num_threads; t++)
		pthread_join(threads[t], NULL);
	pthread_attr_destroy(&attr);
	PTHREAD_LOCK_DESTROY(&W_matrix_lock);
	SG_FREE(parameters);
	SG_FREE(threads);
#else
		klle_weight_matrix_thread((void*)&parameters[t]);
	}
#endif

	return weight_matrix;
}

void* klle_weight_matrix_thread(void* params)
{
	thread_parameters_t* parameters = (thread_parameters_t*)params;
	int thread = parameters->thread;
	int num_threads = parameters->num_threads;
	int k = parameters->k;
	int N = parameters->N;
	const int* neighborhood_matrix = parameters->neighborhood_matrix;
	double (*kernel)(int, int, const void*) = parameters->kernel;
	double* W_matrix = parameters->W_matrix;
	const void* user_data = parameters->user_data;
	double reconstruction_shift = 1e-3;
#ifdef HAVE_PTHREAD
	PTHREAD_LOCK_T* W_matrix_lock = parameters->pthread_lock;
#endif

	int i,q,p;
	double norming, trace, kernel_value;

	double* thread_gram_matrix;// = SG_MALLOC(double, k*k);
	double* rhs_vector;// = SG_MALLOC(double, k);
	double* neighbor_dots;// = SG_MALLOC(double, k);

	for (i=thread; i<N; i+=num_threads)
	{
		kernel_value = kernel(i,i,user_data);
		for (q=0; q<k; q++)
			neighbor_dots[q] = kernel(i,neighborhood_matrix[i*k+q],user_data);

		for (q=0; q<k; q++)
		{
			for (p=0; p<k; p++)
			{
				thread_gram_matrix[q*k+p] = 
					kernel_value - neighbor_dots[q] - neighbor_dots[p] +
					kernel(neighborhood_matrix[i*k+q],neighborhood_matrix[i*k+p],user_data);
			}
		}

		for (q=0; q<k; q++)
			rhs_vector[q] = 1.0;

		// compute tr(C)
		if (reconstruction_shift != 0.0)
		{
			trace = 0.0;
			for (q=0; q<k; q++)
				trace += thread_gram_matrix[q*k+q];
		
			// regularize gram matrix
			for (q=0; q<k; q++)
				thread_gram_matrix[q*k+q] += reconstruction_shift*trace;
		}

		//clapack_dposv(CblasColMajor,CblasLower,k,1,thread_gram_matrix,k,rhs_vector,k);

		// normalize weights
		norming=0.0;
		for (q=0; q<k; q++)
			norming += rhs_vector[q];

		//cblas_dscal(k,1.0/norming,rhs_vector,1);

		memset(thread_gram_matrix,0,sizeof(double)*k*k);
		//cblas_dger(CblasColMajor,k,k,1.0,rhs_vector,1,
		//           rhs_vector,1,thread_gram_matrix,k);

		// put weights into W matrix
		W_matrix[N*i+i] += 1.0;
		for (q=0; q<k; q++)
		{
			W_matrix[N*i+neighborhood_matrix[i*k+q]] -= rhs_vector[q];
			W_matrix[N*neighborhood_matrix[i*k+q]+i] -= rhs_vector[q];
		}
#ifdef HAVE_PTHREAD
		PTHREAD_LOCK(W_matrix_lock);
#endif
		for (q=0; q<k; q++)
		{
			for (p=0; p<k; p++)
				W_matrix[N*neighborhood_matrix[i*k+q]+neighborhood_matrix[i*k+p]]+=
				    thread_gram_matrix[q*k+p];
		}

#ifdef HAVE_PTHREAD
		PTHREAD_UNLOCK(W_matrix_lock);
#endif
	}

	//SG_FREE(thread_gram_matrix);
	//SG_FREE(rhs_vector);
	//SG_FREE(neighbor_dots);

	return NULL;
}

double* kltsa_weight_matrix(
		int* neighborhood_matrix,
		int N,
		int k,
		int matrix_k,
		int target_dimension,
		int num_threads,
		double (*kernel)(int, int, const void*),
		const void* user_data)
{
	int t;
	// allocate threads
	pthread_t* threads;// = SG_MALLOC(pthread_t, num_threads);
	thread_parameters_t* parameters = 
		new thread_parameters_t[num_threads];
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	//PTHREAD_LOCK_T W_matrix_lock;
	//PTHREAD_LOCK_INIT(&W_matrix_lock);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	// init storages to be used
	double* weight_matrix;// = SG_CALLOC(double, N*N);

	for (t=0; t<num_threads; t++)
	{
		parameters[t].thread = t;
		parameters[t].num_threads = num_threads;
		parameters[t].k = k;
		parameters[t].N = N;
		parameters[t].target_dimension = target_dimension;
		parameters[t].neighborhood_matrix = neighborhood_matrix;
		parameters[t].kernel = kernel;
		parameters[t].W_matrix = weight_matrix;
		parameters[t].user_data = user_data;
#ifdef HAVE_PTHREAD
		parameters[t].pthread_lock = &W_matrix_lock;
		pthread_create(&threads[t], &attr, kltsa_weight_matrix_thread, (void*)&parameters[t]);
	}
	for (t=0; t<num_threads; t++)
		pthread_join(threads[t], NULL);
	pthread_attr_destroy(&attr);
	PTHREAD_LOCK_DESTROY(&W_matrix_lock);
	SG_FREE(parameters);
	SG_FREE(threads);
#else
		kltsa_weight_matrix_thread((void*)&parameters[t]);
	}
#endif

	//for (int i=0; i<N; i++)
	//	for (int q=0; q<k; q++)
	//		weight_matrix[N*neighborhood_matrix[i*k+q]+neighborhood_matrix[i*k+q]] += 1.0;

	//CMath::display_matrix(weight_matrix,N,N,"W");
	return weight_matrix;
}

void* kltsa_weight_matrix_thread(void* params)
{
	thread_parameters_t* parameters = (thread_parameters_t*)params;
	int thread = parameters->thread;
	int num_threads = parameters->num_threads;
	int k = parameters->k;
	int target_dim = parameters->target_dimension;
	int N = parameters->N;
	const int* neighborhood_matrix = parameters->neighborhood_matrix;
	double (*kernel)(int, int, const void*) = parameters->kernel;
	double* W_matrix = parameters->W_matrix;
	const void* user_data = parameters->user_data;
#ifdef HAVE_PTHREAD
	PTHREAD_LOCK_T* W_matrix_lock = parameters->pthread_lock;
#endif

	int32_t i,q,p;

	double* G_matrix;// = SG_CALLOC(double, k*(1+target_dim));
	double* ev_vector;// = SG_CALLOC(double, k);
	double* thread_gram_matrix;// = SG_MALLOC(double, k*k);
	
	for (q=0; q<k; q++)
		G_matrix[q] = 1.0/sqrt((double)k);

	for (i=thread; i<N; i+=num_threads)
	{
		for (q=0; q<k; q++)
		{
			for (p=0; p<k; p++)
			{
				thread_gram_matrix[q*k+p] = 
					kernel(neighborhood_matrix[i*k+q],neighborhood_matrix[i*k+p],user_data);
			}
		}

		//CMath::display_matrix(thread_gram_matrix,k,k,"TKM");

		//CMath::center_matrix(thread_gram_matrix,k,k);

		int32_t info = 0; 
		//wrap_dsyevr('V','U',k,thread_gram_matrix,k,k-target_dim+1,k,ev_vector,G_matrix+k,&info);
		//ASSERT(info==0);
		
		//cblas_dgemm(CblasColMajor,CblasNoTrans,CblasTrans,
		//            k,k,1+target_dim,
		//            1.0,G_matrix,k,
		//                G_matrix,k,
		//            0.0,thread_gram_matrix,k);
		
		//CMath::display_matrix(thread_gram_matrix,k,k,"TKM");

#ifdef HAVE_PTHREAD
		PTHREAD_LOCK(W_matrix_lock);
#endif
		for (q=0; q<k; q++)
			W_matrix[N*neighborhood_matrix[i*k+q]+neighborhood_matrix[i*k+q]] += 1.0;

		for (q=0; q<k; q++)
		{
			for (p=0; p<k; p++)
				W_matrix[N*neighborhood_matrix[i*k+p]+neighborhood_matrix[i*k+q]] -= thread_gram_matrix[p*k+q];
		}
#ifdef HAVE_PTHREAD
		PTHREAD_UNLOCK(W_matrix_lock);
#endif
	}
	//SG_FREE(G_matrix);
	//SG_FREE(thread_gram_matrix);
	//SG_FREE(ev_vector);

	return NULL;
}