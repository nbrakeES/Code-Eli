/*********************************************************************************
* Copyright (c) 2013 David D. Marshall <ddmarsha@calpoly.edu>
*
* All rights reserved. This program and the accompanying materials
* are made available under the terms of the Eclipse Public License v1.0
* which accompanies this distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*
* Contributors:
*    David D. Marshall - initial code and implementation
********************************************************************************/

#ifndef eli_geom_intersect_minimum_distance_curve_hpp
#define eli_geom_intersect_minimum_distance_curve_hpp

#include <cmath>
#include <vector>

#include "Eigen/Eigen"

#include "eli/mutil/nls/newton_raphson_shacham_method.hpp"

#include "eli/geom/point/distance.hpp"
#include "eli/geom/intersect/minimum_distance_line.hpp"

namespace eli
{
  namespace geom
  {
    namespace intersect
    {
      namespace internal
      {
        template <typename curve__>
        struct curve_g_functor
        {
          const curve__ *pc;
          typename curve__::point_type pt;

          typename curve__::data_type operator()(const typename curve__::data_type &t) const
          {
            typename curve__::data_type tt(t);

            // if closed curve then wrap t
            if (pc->closed())
            {
              if (t<0)
              {
                while(tt<0)
                {
                  tt+=1;
                }
              }
              else if (t>1)
              {
                while(tt>1)
                {
                  tt-=1;
                }
              }
              assert((tt>-0.001) && (tt<1.001));
            }
            else
            {
              assert((tt>-0.001) && (tt<1.001));
            }

            tt=std::min(std::max(tt, static_cast<typename curve__::data_type>(0)), static_cast<typename curve__::data_type>(1));

            return (pc->f(tt)-pt).dot(pc->fp(tt));
          }
        };

        template <typename curve__>
        struct curve_gp_functor
        {
          const curve__ *pc;
          typename curve__::point_type pt;

          typename curve__::data_type operator()(const typename curve__::data_type &t) const
          {
            typename curve__::data_type tt(t);

            // if closed curve then wrap t
            if (pc->closed())
            {
              if (t<0)
              {
                while(tt<0)
                {
                  tt+=1;
                }
              }
              else if (t>1)
              {
                while(tt>1)
                {
                  tt-=1;
                }
              }
              assert((tt>-0.001) && (tt<1.001));
            }
            else
            {
              assert((tt>-0.001) && (tt<1.001));
            }

            tt=std::min(std::max(tt, static_cast<typename curve__::data_type>(0)), static_cast<typename curve__::data_type>(1));

            typename curve__::point_type fp(pc->fp(tt));

            return fp.dot(fp)+pc->fpp(tt).dot(pc->f(tt)-pt);
          }
        };
      }

      template<typename curve__>
      typename curve__::data_type minimum_distance(typename curve__::data_type &t, const curve__ &c, const typename curve__::point_type &pt)
      {
        typename curve__::tolerance_type tol;
        eli::mutil::nls::newton_raphson_shacham_method<typename curve__::data_type> nrm;
        typename eli::mutil::nls::newton_raphson_shacham_method<typename curve__::data_type>::status stat;
        internal::curve_g_functor<curve__> g;
        internal::curve_gp_functor<curve__> gp;

        // setup the functors
        g.pc=&c;
        g.pt=pt;
        gp.pc=&c;
        gp.pt=pt;

        // possible that end points are closest, so start by checking them
        typename curve__::data_type d0, d1, dist;

        d0=eli::geom::point::distance(c.f(0), pt);
        d1=eli::geom::point::distance(c.f(1), pt);

        // start is closest
        if (d0<=d1)
        {
          t=0;
          dist=d0;
        }
        else
        {
          t=1;
          dist=d1;
        }

        // setup the solver
        nrm.set_absolute_tolerance(tol.get_absolute_tolerance());
        nrm.set_max_iteration(100);
        if (c.open())
        {
          nrm.set_lower_condition(0, eli::mutil::nls::newton_raphson_shacham_method<typename curve__::data_type>::NRS_EXCLUSIVE);
          nrm.set_upper_condition(1, eli::mutil::nls::newton_raphson_shacham_method<typename curve__::data_type>::NRS_EXCLUSIVE);
        }

        // need to pick initial guesses
        typename curve__::index_type i, deg(c.degree());
        typename curve__::point_type p0, p1;
        std::vector<typename curve__::data_type> tsample(deg+1), tinit;
        typename curve__::data_type temp, tlen;

        // determine the sample parameters from the control polygon points
        i=0;
        p1=c.get_control_point(i);
        tsample[i]=0;
        for (++i; i<=deg; ++i)
        {
          p0=p1;
          p1=c.get_control_point(i);
          tsample[i]=tsample[i-1]+eli::geom::point::distance(p0, p1);
        }
        tlen=tsample[deg];

        // find candidate starting locations from linear approximations to curve
        i=0;
        p1=c.f(tsample[i]/tlen);
        for (++i; i<=deg; ++i)
        {
          p0=p1;
          p1=c.f(tsample[i]/tlen);
          minimum_distance(temp, p0, p1-p0, pt);
//           std::cout << "panel #=" << i << "\tt_temp=" << temp << "\tp0=" << p0 << "\tp1=" << p1 << std::endl;

          if ((temp>=0) && (temp<=1))
          {
            tinit.push_back(std::max(static_cast<typename curve__::data_type>(0),
                                     std::min(static_cast<typename curve__::data_type>(1),
                                              (tsample[i-1]+(tsample[i]-tsample[i-1])*temp)/tlen)));
          }
        }

        // if found no candidates using linear approximation then try points
        if (tinit.empty())
        {
          // find candidate starting locations using distance between sampled points on curve and point
          for (i=0; i<=deg; ++i)
          {
            temp=eli::geom::point::distance(c.f(tsample[i]/tlen), pt);
//             std::cout << "point #=" << i << "\tdist_temp=" << temp << std::endl;
            if (temp<=1.01*dist)
            {
              tinit.push_back(tsample[i]/tlen);
//               std::cout << "added point #=" << i << "\twith t=" << tsample[i]/tlen << std::endl;
            }
          }
        }

//         std::cout << "# t guesses=" << tinit.size() << std::endl;

        // make sure have some solutions to iterate
        tinit.push_back(0);
        if (c.open())
        {
          tinit.push_back(1);
        }

        // cycle through all possible minima to find best
        for (size_t j=0; j<tinit.size(); ++j)
        {
          nrm.set_initial_guess(tinit[j]);

          // find the root
          stat = nrm.find_root(d0, g, gp, 0);

          // if found root then make sure it is within bounds and return
          if (stat==eli::mutil::nls::newton_raphson_method<typename curve__::data_type>::converged)
          {
            // if have valid solution
            if ((d0>=0) && (d0<=1))
            {
              d1=eli::geom::point::distance(c.f(d0), pt);

              // check to see if is closer than previous minimum
              if (d1<dist)
              {
                t=d0;
                dist=d1;
              }
            }
          }
          else
          {
//             std::cout << "not converged!" << std::endl;
          }
        }

        return dist;
      }
    }
  }
}
#endif
