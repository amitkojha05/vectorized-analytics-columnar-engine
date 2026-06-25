#pragma once
#include <functional>
#include <vector>

enum class NodeType {
    SCAN,
    FILTER,
    AGGREGATE
};

struct ExecNode {
    NodeType              type;
    std::function<void()> execute;
};

class PipelineExecutor {
public:
    std::vector<ExecNode> nodes;

    void execute() {
        for (auto& n : nodes) {
            n.execute();
        }
    }
};
