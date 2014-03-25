#define BOOST_NO_CXX11_RVALUE_REFERENCES

#include <stdio.h>
#include <gt_assert.h>
#include <arg_type.h>
#include <domain_type.h>
#include <storage.h>
#include <boost/mpl/for_each.hpp>

using namespace gridtools;

struct print {
    mutable int count;
    mutable bool result;

    print(void)
        : count(0)
        , result(true)
    {}

    template <typename T>
    void operator()(T const& v) const {
#ifndef NDEBUG
        std::cout << T::value << std::endl;
#endif
        if (T::value != count)
            result == false;
        ++count;
    }
};

struct print_pretty {
    template <typename T>
    void operator()(T const& v) const {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
    }
};

bool test_domain_indices() {
    typedef gridtools::storage<double, gridtools::layout_map<0,1,2> > storage_type;

    int d1 = 10;
    int d2 = 10;
    int d3 = 10;

    storage_type in(d1,d2,d3,-1, std::string("in"));
    storage_type out(d1,d2,d3,-7.3, std::string("out"));
    storage_type coeff(d1,d2,d3,8, std::string("coeff"));

    typedef arg<2, gridtools::temporary<storage_type> > p_lap;
    typedef arg<1, gridtools::temporary<storage_type> > p_flx;
    typedef arg<5, gridtools::temporary<storage_type> > p_fly;
    typedef arg<0, storage_type > p_coeff;
    typedef arg<3, storage_type > p_in;
    typedef arg<4, storage_type > p_out;


    typedef boost::mpl::vector<p_lap, p_flx, p_fly, p_coeff, p_in, p_out> arg_type_list;

    gridtools::domain_type<arg_type_list> domain
        (boost::fusion::make_vector(&out, &in, &coeff /*,&fly, &flx*/));

    bool result = true;

#ifndef NDEBUG
    boost::mpl::for_each<gridtools::domain_type<arg_type_list>::raw_index_list>(print());
    std::cout << std::endl;
    boost::mpl::for_each<gridtools::domain_type<arg_type_list>::range_t>(print());
    std::cout << std::endl;
    boost::mpl::for_each<gridtools::domain_type<arg_type_list>::arg_list_mpl>(print_pretty());
#endif
    std::cout << std::endl;
    print pf;
    boost::mpl::for_each<gridtools::domain_type<arg_type_list>::index_list>(pf);
    return pf.result;
}
