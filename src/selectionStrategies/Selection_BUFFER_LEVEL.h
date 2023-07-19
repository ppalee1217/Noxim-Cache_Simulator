#ifndef __NOXIMSELECTION_BUFFER_LEVEL_H__
#define __NOXIMSELECTION_BUFFER_LEVEL_H__

#include "SelectionStrategy.h"
#include "SelectionStrategies.h"
#include "../Router.h"
#include "../DataRouter.h"

using namespace std;

class Selection_BUFFER_LEVEL : SelectionStrategy {
	public:
        int apply(Router * router, const vector < int >&directions, const RouteData & route_data);
        void perCycleUpdate(Router * router);
		int apply(DataRouter * router, const vector < int >&directions, const RouteData & route_data);
        void perCycleUpdate(DataRouter * router);

		static Selection_BUFFER_LEVEL * getInstance();

	private:
		Selection_BUFFER_LEVEL(){};
		~Selection_BUFFER_LEVEL(){};

		static Selection_BUFFER_LEVEL * selection_BUFFER_LEVEL;
		static SelectionStrategiesRegister selectionStrategiesRegister;
};

#endif
