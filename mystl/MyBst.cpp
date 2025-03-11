//
// Created by Kolane on 2025/3/12.
//

#include <iostream>
using namespace std;

class BST {
private:
    struct Node {
        int key;
        Node *left, *right;
        Node(int k) : key(k), left(nullptr), right(nullptr) {}
    };

    Node* root;

    // 递归插入辅助函数
    Node* insert(Node* node, int key) {
        if (!node) return new Node(key);
        if (key < node->key)
            node->left = insert(node->left, key);
        else if (key > node->key)
            node->right = insert(node->right, key);
        return node;
    }

    // 递归删除辅助函数
    Node* deleteNode(Node* node, int key) {
        if (!node) return nullptr;

        if (key < node->key) {
            node->left = deleteNode(node->left, key);
        } else if (key > node->key) {
            node->right = deleteNode(node->right, key);
        } else {
            // Case 1/2: 单子节点或无子节点
            if (!node->left) {
                Node* temp = node->right;
                delete node;
                return temp;
            } else if (!node->right) {
                Node* temp = node->left;
                delete node;
                return temp;
            }
            // Case 3: 双子节点（使用后继节点）
            Node* successor = node->right;
            while (successor->left)
                successor = successor->left;
            node->key = successor->key;
            node->right = deleteNode(node->right, successor->key);
        }
        return node;
    }

    // 递归销毁树
    void destroyTree(Node* node) {
        if (node) {
            destroyTree(node->left);
            destroyTree(node->right);
            delete node;
        }
    }

public:
    BST() : root(nullptr) {}
    ~BST() { destroyTree(root); }

    void insert(int key) { root = insert(root, key); }

    void remove(int key) { root = deleteNode(root, key); }

    bool search(int key) {
        Node* curr = root;
        while (curr) {
            if (key == curr->key) return true;
            curr = (key < curr->key) ? curr->left : curr->right;
        }
        return false;
    }

    // 可视化打印（前序遍历）
    void print(Node* node = nullptr, int depth = 0) {
        if (!node) {
            if (depth == 0) node = root;
            else return;
        }
        cout << string(depth*2, ' ') << node->key << endl;
        if (node->left) print(node->left, depth+1);
        if (node->right) print(node->right, depth+1);
    }
};

// 使用示例
int main() {
    BST tree;
    int nums[] = {8, 3, 10, 1, 6, 14, 4, 7, 13};
    for (int n : nums) tree.insert(n);

    cout << "原始树结构：" << endl;
    tree.print(); // 展示树形结构

    tree.remove(6);
    cout << "\n删除6后的结构：" << endl;
    tree.print();

    return 0;
}
