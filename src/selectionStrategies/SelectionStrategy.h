#ifndef __NOXIMSELECTIONSTRATEGY_H__
#define __NOXIMSELECTIONSTRATEGY_H__

#include <vector>
#include "../DataStructs.h"
#include "../Utils.h"

using namespace std;

struct Router;
struct  DataRouter;

class SelectionStrategy
{
        public:
        virtual int apply(Router * router, const vector < int >&directions, const RouteData & route_data) = 0;
        virtual void perCycleUpdate(Router * router) = 0;
        virtual int apply(DataRouter * router, const vector < int >&directions, const RouteData & route_data) = 0;
        virtual void perCycleUpdate(DataRouter * router) = 0;
};

#endif
