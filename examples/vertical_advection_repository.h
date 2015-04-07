#pragma once

#include <gridtools.h>
#include "vertical_advection_defs.h"

namespace vertical_advection {

class repository
{
public:
    typedef gridtools::layout_map<0,1,2> layout_ijk;
    typedef gridtools::layout_map<0,1,-1> layout_ij;
    typedef gridtools::layout_map<-1,-1,-1> layout_scalar;

    typedef va_backend::storage_type<gridtools::float_type, layout_ijk >::type storage_type;
    typedef va_backend::storage_type<gridtools::float_type, layout_ij >::type ij_storage_type;

    typedef va_backend::storage_type<gridtools::float_type, layout_scalar >::type scalar_storage_type;
    typedef va_backend::temporary_storage_type<gridtools::float_type, layout_ijk >::type tmp_storage_type;
    typedef va_backend::temporary_storage_type<gridtools::float_type, layout_scalar>::type tmp_scalar_storage_type;

    repository(const uint_t idim, const uint_t jdim, const uint_t kdim, const uint_t halo_size) :
        utens_stage_(idim, jdim, kdim, -1., "utens_stage"),
        utens_stage_ref_(idim, jdim, kdim, -1., "utens_stage_ref"),
        u_stage_(idim, jdim, kdim, -1., "u_stage"),
        wcon_(idim, jdim, kdim, -1., "wcon"),
        u_pos_(idim, jdim, kdim, -1., "u_pos"),
        utens_(idim, jdim, kdim, -1., "utens"),
        ipos_(idim, jdim, kdim, -1., "ipos"),
        jpos_(idim, jdim, kdim, -1., "jpos"),
        kpos_(idim, jdim, kdim, -1., "kpos"),
        //dtr_stage_(0,0,0, -1, "dtr_stage"),
        dtr_stage_(idim,jdim,kdim, -1., "dtr_stage"),
        halo_size_(halo_size),
        idim_(idim), jdim_(jdim), kdim_(kdim)
    {}

    void init_fields()
    {
        // set the fields to advect
        const double PI = std::atan(1.)*4.;

        const uint_t i_begin = 0;
        const uint_t i_end=  idim_;
        const uint_t j_begin = 0;
        const uint_t j_end=  jdim_;
        const uint_t k_begin = 0;
        const uint_t k_end=  kdim_;

        double dtadv = (double)20./3.;
        dtr_stage_(0,0,0) = (double)1.0 / dtadv;

        double dx = 1./(double)(i_end-i_begin);
        double dy = 1./(double)(j_end-j_begin);
        double dz = 1./(double)(k_end-k_begin);

        for( int j=j_begin; j<j_end; j++ ){
            for( int i=i_begin; i<i_end; i++ ){
                double x = dx*(double)(i-i_begin);
                double y = dy*(double)(j-j_begin);
                for( int k=k_begin; k<k_end; k++ )
                {
                    double z = dz*(double)(k-k_begin);
                    dtr_stage_(i,j,k) = (double)1.0 / dtadv;

                    // u values between 5 and 9
                    u_stage_(i,j,k) = 5. + 4*(2.+cos(PI*(x+y)) + sin(2*PI*(x+y)))/4.;
                    u_pos_(i,j,k) = 5. + 4*(2.+cos(PI*(x+y)) + sin(2*PI*(x+y)))/4.;
                    // utens values between -3e-6 and 3e-6 (with zero at k=0)
                    utens_(i,j,k) = 3e-6*(-1 + 2*(2.+cos(PI*(x+y)) + cos(PI*y*z))/4.+ 0.05*(0.5-double(rand()%100)/50.));
                    // wcon values between -2e-4 and 2e-4 (with zero at k=0)
                    wcon_(i,j,k) = 2e-4*(-1 + 2*(2.+cos(PI*(x+z)) + cos(PI*y))/4. + 0.1*(0.5-double(rand()%100)/50.));

                    utens_stage_(i,j,k) = 7. + 5*(2.+cos(PI*(x+y)) + sin(2*PI*(x+y)))/4. + k*0.1;
                    utens_stage_ref_(i,j,k) = utens_stage_(i,j,k);
                    ipos_(i,j,k) = i;
                    jpos_(i,j,k) = j;
                    kpos_(i,j,k)=k;
                }
            }
        }

    }

    template<typename TStorage_type, typename TValue_type>
    void init_field_to_value(TStorage_type& field, TValue_type value)
    {

        const bool has_dim0 = TStorage_type::basic_type::layout::template at<0>() != -1;
        const bool has_dim1 = TStorage_type::basic_type::layout::template at<1>() != -1;
        const bool has_dim2 = TStorage_type::basic_type::layout::template at<2>() != -1;
        for(int k=0; k < (has_dim2 ? kdim_ : 1); ++k)
        {
            for (int i = 0; i < (has_dim0 ? idim_ : 1); ++i)
            {
                for (int j = 0; j < (has_dim1 ? jdim_ : 1); ++j)
                {
                    field(i,j,k) = value;
                }
            }
        }
    }

    void generate_reference()
    {
        double dtr_stage = dtr_stage_(0,0,0);

        ij_storage_type datacol(idim_, jdim_, (uint_t)1, -1., "datacol");
        storage_type ccol(idim_, jdim_, kdim_, -1., "ccol"), dcol(idim_, jdim_, kdim_, -1., "dcol");

        init_field_to_value(ccol, 0.0);
        init_field_to_value(dcol, 0.0);
        std::cout << "Call D " << std::endl;
        init_field_to_value(datacol, 0.0);
        std::cout << "After " << std::endl;

        //Generate U
        forward_sweep(1, 0, ccol, dcol);
        backward_sweep(ccol, dcol, datacol);
    }

    void forward_sweep(int ishift, int jshift, storage_type& ccol, storage_type& dcol)
    {
        double dtr_stage = dtr_stage_(0,0,0);
        // k minimum
        int k = 0;
        for(int i=halo_size_; i < idim_-halo_size_; ++i)
        {
            for(int j=halo_size_; j < jdim_-halo_size_; ++j)
            {
                double gcv = (double) 0.25 * (wcon_(i+ishift, j+jshift, k+1) + wcon_(i, j, k+1));
                double cs = gcv * BET_M;

                ccol(i, j, k) = gcv * BET_P;
                double bcol = dtr_stage_(0,0,0) - ccol(i, j, k);

                // update the d column
                double correctionTerm = -cs * (u_stage_(i, j, k + 1) - u_stage_(i, j, k));
                dcol(i, j, k) = dtr_stage * u_pos_(i, j, k) + utens_(i, j, k)
                        + utens_stage_ref_(i, j, k) + correctionTerm;

                double divided = (double) 1.0 / bcol;
                ccol(i, j, k) = ccol(i, j, k) * divided;
                dcol(i, j, k) = dcol(i, j, k) * divided;

                if(i==3 && j == 3)
                std::cout << "AT ref at  " << k << "  " << bcol << "  " << ccol(i,j,k) << " " << dcol(i,j,k) << "  " << gcv <<
                "  " << wcon_(i,j,k+1) << "  " << wcon_(i+ishift, j+jshift, k+1) << std::endl;

            }
        }

        //kbody
        for( k=1; k < kdim_-1; ++k)
        {
            for (int i = halo_size_; i < idim_-halo_size_; ++i)
            {
                for (int j = halo_size_; j < jdim_-halo_size_; ++j)
                {
                    double gav = (double)-0.25 * (wcon_(i+ishift,j+jshift,k) + wcon_(i,j,k));
                    double gcv = (double)0.25 * (wcon_(i+ishift,j+jshift,k+1) + wcon_(i,j,k+1));

                    double as = gav * BET_M;
                    double cs = gcv * BET_M;

                    double acol = gav * BET_P;
                    ccol(i,j,k) = gcv * BET_P;
                    double bcol = dtr_stage - acol - ccol(i,j,k);

                    double correctionTerm = -as * (u_stage_(i,j,k-1) - u_stage_(i,j,k)) -
                            cs * (u_stage_(i,j,k+1) - u_stage_(i,j,k));
                    dcol(i, j, k) = dtr_stage * u_pos_(i, j, k) + utens_(i, j, k)
                            + utens_stage_ref_(i, j, k) + correctionTerm;

                    double divided = (double)1.0 / (bcol - (ccol(i,j,k-1) * acol));
                    ccol(i,j,k) = ccol(i,j,k) * divided;
                    dcol(i,j,k) = (dcol(i,j,k) - (dcol(i,j,k-1) * acol)) * divided;
                    if(i==3 && j == 3)
                    std::cout << "FORDW REF at  " << k << "  " << acol << "  " << bcol << "  " << ccol(i,j,k) << " " << dcol(i,j,k) << std::endl;
                }
            }
        }

        // k maximum
        k=kdim_-1;
        for (int i = halo_size_; i < idim_-halo_size_; ++i)
        {
            for (int j = halo_size_; j < jdim_-halo_size_; ++j)
            {
                double gav = -(double)0.25 * (wcon_(i+ishift,j+jshift,k)+wcon_(i,j,k) );
                double as = gav * BET_M;

                double acol = gav * BET_P;
                double bcol = dtr_stage - acol;

                // update the d column
                double correctionTerm = -as*(u_stage_(i,j,k-1) - u_stage_(i,j,k));
                dcol(i, j, k) = dtr_stage * u_pos_(i, j, k) + utens_(i, j, k)
                                        + utens_stage_ref_(i, j, k) + correctionTerm;

                double divided = (double)1.0 / (bcol - (ccol(i,j,k-1) * acol));
                dcol(i,j,k) = (dcol(i,j,k) - (dcol(i,j,k-1) * acol)) * divided;
            }
        }
    }

    void update_cpu()
    {
#ifdef CUDA_EXAMPLE
        utens_stage_.data().update_cpu();
#endif
    }

    void backward_sweep(storage_type& ccol, storage_type& dcol, ij_storage_type& datacol)
    {
        double dtr_stage = dtr_stage_(0,0,0);
        // k maximum
        int k=kdim_-1;
        for (int i = halo_size_; i < idim_-halo_size_; ++i)
        {
            for (int j = halo_size_; j < jdim_-halo_size_; ++j)
            {
                datacol(i,j,k) = dcol(i,j,k);
                ccol(i,j,k) = datacol(i,j,k);
                utens_stage_ref_(i,j,k) = dtr_stage*(datacol(i,j,k) - u_pos_(i,j,k));
            }
        }
        // kbody
        for(k=kdim_-2; k >= 0; --k)
        {
            for (int i = halo_size_; i < idim_-halo_size_; ++i)
            {
                for (int j = halo_size_; j < jdim_-halo_size_; ++j)
                {
                    datacol(i,j,k) = dcol(i,j,k) - (ccol(i,j,k)*datacol(i,j,k+1));
                    ccol(i,j,k) = datacol(i,j,k);
                    utens_stage_ref_(i,j,k) = dtr_stage*(datacol(i,j,k) - u_pos_(i,j,k));
                }
            }
        }
    }

    storage_type& utens_stage() {return utens_stage_;}
    storage_type& wcon() {return wcon_;}
    storage_type& u_pos() {return u_pos_;}
    storage_type& utens() {return utens_;}
    storage_type& ipos() {return ipos_;}
    storage_type& jpos() {return jpos_;}
    storage_type& kpos() {return kpos_;}
    //TODO fix this, with a scalar storage the GPU version does not work
    //scalar_storage_type& dtr_stage()
    storage_type& dtr_stage() {return dtr_stage_;}

    //output fields
    storage_type& u_stage() {return u_stage_;}
    storage_type& utens_stage_ref() {return utens_stage_ref_;}
private:
    storage_type utens_stage_, u_stage_, wcon_, u_pos_, utens_, utens_stage_ref_;
    storage_type ipos_, jpos_, kpos_;
    //scalar_storage_type dtr_stage_;
    storage_type dtr_stage_;
    const int halo_size_;
    const int idim_, jdim_, kdim_;
};

}