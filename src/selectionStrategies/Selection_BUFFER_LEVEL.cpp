#include "Selection_BUFFER_LEVEL.h"

SelectionStrategiesRegister Selection_BUFFER_LEVEL::selectionStrategiesRegister("BUFFER_LEVEL", getInstance());

Selection_BUFFER_LEVEL * Selection_BUFFER_LEVEL::selection_BUFFER_LEVEL = 0;

Selection_BUFFER_LEVEL * Selection_BUFFER_LEVEL::getInstance() {
	if ( selection_BUFFER_LEVEL == 0 )
		selection_BUFFER_LEVEL = new Selection_BUFFER_LEVEL();
    
	return selection_BUFFER_LEVEL;
}

int Selection_BUFFER_LEVEL::apply(Router * router, const vector < int >&directions, const RouteData & route_data){
    vector < int >best_dirs;
    int max_free_slots = 0;
    for (unsigned int i = 0; i < directions.size(); i++) {

	bool available = false;

	int free_slots = router->free_slots_neighbor[directions[i]].read();

	try {
	    available = router->reservation_table.isNotReserved(directions[i]);
	}
	catch (int error)
	{
	    if (error==NOT_VALID) continue;
	    assert(false);
	}


	if (available) {
	    if (free_slots > max_free_slots) {
		max_free_slots = free_slots;
		best_dirs.clear();
		best_dirs.push_back(directions[i]);
	    } else if (free_slots == max_free_slots)
		best_dirs.push_back(directions[i]);
	}
    }

    if (best_dirs.size())
	return (best_dirs[rand() % best_dirs.size()]);
    else
	return (directions[rand() % directions.size()]);

    //-------------------------
    // TODO: unfair if multiple directions have same buffer level
    // TODO: to check when both available

}

void Selection_BUFFER_LEVEL::perCycleUpdate(Router * router) {
	    // update current input buffers level to neighbors
	    for (int i = 0; i < DIRECTIONS + 1; i++)
		router->free_slots[i].write(router->buffer[i][DEFAULT_VC].getCurrentFreeSlots());

	    // NoP selection: send neighbor info to each direction 'i'
	    NoP_data current_NoP_data = router->getCurrentNoPData();

	    for (int i = 0; i < DIRECTIONS; i++)
		router->NoP_data_out[i].write(current_NoP_data);
}


// Data NoC - AddDate: 2023/04/29
int Selection_BUFFER_LEVEL::apply(DataRouter * router, const vector < int >&directions, const RouteData & route_data){
    vector < int >best_dirs;
    int max_free_slots = 0;
    for (unsigned int i = 0; i < directions.size(); i++) {

	bool available = false;

	int free_slots = router->free_slots_neighbor[directions[i]].read();

	try {
	    available = router->reservation_table.isNotReserved(directions[i]);
	}
	catch (int error)
	{
	    if (error==NOT_VALID) continue;
	    assert(false);
	}


	if (available) {
	    if (free_slots > max_free_slots) {
		max_free_slots = free_slots;
		best_dirs.clear();
		best_dirs.push_back(directions[i]);
	    } else if (free_slots == max_free_slots)
		best_dirs.push_back(directions[i]);
	}
    }

    if (best_dirs.size())
	return (best_dirs[rand() % best_dirs.size()]);
    else
	return (directions[rand() % directions.size()]);
}

void Selection_BUFFER_LEVEL::perCycleUpdate(DataRouter * router) {
	    // update current input buffers level to neighbors
	    for (int i = 0; i < DIRECTIONS + 1; i++)
			router->free_slots[i].write(router->buffer[i][DEFAULT_VC].getCurrentFreeSlots());

	    // NoP selection: send neighbor info to each direction 'i'
	    NoP_data current_NoP_data = router->getCurrentNoPData();

	    for (int i = 0; i < DIRECTIONS; i++)
			router->NoP_data_out[i].write(current_NoP_data);
}