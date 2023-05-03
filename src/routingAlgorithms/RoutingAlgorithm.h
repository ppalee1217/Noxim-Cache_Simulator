#ifndef __NOXIMROUTINGALGORITHM_H__
#define __NOXIMROUTINGALGORITHM_H__

#include <vector>
#include "../DataStructs.h"
#include "../Utils.h"

using namespace std;

struct Router;
struct DataRouter;

class RoutingAlgorithm
{
	public:
		virtual vector<int> route(Router * router, const RouteData & routeData) = 0;
		virtual vector<int> route(DataRouter * router, const RouteData & routeData) = 0;
};

#endif
