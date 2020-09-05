#include <string>
#include <vector>

#include <interpreter/object.hpp>
#include <pattern_functions.hpp>

enum Event
{
    TIMING_EVENT,
    PROCESS_UPDATE_EVENT,

};

struct event_queue_item
{
    unsigned int type; // timing info or process update
    // timing info
    mixer_timing_info timing_info;
    // process update
    int target_process_id;
    ProcessType process_type;
    ProcessTimerType timer_type;
    float loop_len;
    std::string command;
    std::shared_ptr<ast::Expression> pattern_expression;
    ProcessPatternTarget target_type;
    std::vector<std::string> targets;
    std::string while_condition;
    std::string while_body;
    std::string while_then_body;
    std::vector<std::shared_ptr<PatternFunction>> funcz;
};
