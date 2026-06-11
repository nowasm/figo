#include "figmalib/document.h"

namespace figmalib {

Node* Node::findById(const std::string& nodeId) {
    if (id == nodeId) return this;
    for (auto& c : children) {
        if (Node* n = c->findById(nodeId)) return n;
    }
    return nullptr;
}

Node* Node::findByName(const std::string& nodeName) {
    if (name == nodeName) return this;
    for (auto& c : children) {
        if (Node* n = c->findByName(nodeName)) return n;
    }
    return nullptr;
}

void Node::visit(const std::function<bool(Node&)>& fn) {
    if (!fn(*this)) return;
    for (auto& c : children) c->visit(fn);
}

std::vector<Node*> Document::topLevelFrames() const {
    std::vector<Node*> frames;
    if (!root) return frames;
    auto frameLike = [](NodeType t) {
        return t == NodeType::Frame || t == NodeType::Component || t == NodeType::Instance ||
               t == NodeType::Section;
    };
    for (auto& page : root->children) {
        if (page->type == NodeType::Canvas) {
            for (auto& child : page->children) {
                if (frameLike(child->type)) frames.push_back(child.get());
            }
        } else if (frameLike(page->type) && page->width > 0 && page->height > 0) {
            // canvas.json pages can miss the CANVAS inference; accept direct frames.
            frames.push_back(page.get());
        }
    }
    // A bare node tree (no DOCUMENT/CANVAS wrapper): treat the root as a frame.
    if (frames.empty() && root->type != NodeType::Document) frames.push_back(root.get());
    return frames;
}

}  // namespace figmalib
