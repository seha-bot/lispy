#ifndef NEW_GC_HPP
#define NEW_GC_HPP

#include <utility>
#include <vector>

struct GC {
    int debug() const {
        auto it = m_head;
        int c = 0;
        while (it) {
            ++c;
            it = it->next;
        }
        return c;
    }

    struct Node {
        friend GC;
        virtual ~Node() = default;

        void depend_on(Node *that) { dependencies.push_back(that); }
        void lock() noexcept { locked = true; }
        void unlock() noexcept { locked = false; }

    private:
        std::vector<Node *> dependencies;
        Node *next = nullptr;
        bool locked = false;
        bool marked = false;
    };

    template <typename T>
    struct Ptr {
        Ptr() : m_ptr(nullptr) {}
        Ptr(T *ptr) : m_ptr(ptr) { ptr->lock(); }
        Ptr(Ptr const&) = delete;
        Ptr& operator=(Ptr const&) = delete;

        Ptr(Ptr&& that) noexcept : m_ptr(that.m_ptr) { that.m_ptr = nullptr; }
        Ptr& operator=(Ptr&& that) noexcept {
            std::swap(m_ptr, that.m_ptr);
            return *this;
        }

        ~Ptr() {
            if (m_ptr) {
                m_ptr->unlock();
            }
        }

        T *get() const { return m_ptr; }

    private:
        T *m_ptr;
    };

    template <typename T>
    struct Managed {
        template <typename... Args>
        static Ptr<T> make(GC& gc, Args&&...args) {
            Ptr<T> p(new T(std::forward<Args>(args)...));
            gc.register_(p.get());
            return p;
        }
    };

    GC() = default;
    GC(GC const&) = delete;
    GC& operator=(GC const&) = delete;

    ~GC() {
        while (m_head) {
            auto *curr = m_head;
            m_head = m_head->next;
            delete curr;
        }
    }

    void register_(Node *node) noexcept {
        node->next = m_head;
        node->marked = m_inactive_mark;
        m_head = node;
    }

    void collect() {
        {
            auto *it = m_head;
            while (it) {
                if (it->locked) {
                    traverse(it, not m_inactive_mark);
                }
                it = it->next;
            }
        }

        {
            auto **it = &m_head;
            while (*it) {
                if ((*it)->marked == m_inactive_mark) {
                    auto *curr = *it;
                    (*it) = (*it)->next;
                    delete curr;
                } else {
                    it = &(*it)->next;
                }
            }
        }

        m_inactive_mark = not m_inactive_mark;
    }

private:
    void traverse(Node *node, bool mark) {
        std::vector<Node *> stack{node};

        while (not stack.empty()) {
            node = stack.back();
            stack.pop_back();

            if (node->marked == mark) {
                continue;
            }
            node->marked = mark;

            stack.insert(stack.end(), node->dependencies.begin(), node->dependencies.end());
        }
    }

    bool m_inactive_mark = false;
    Node *m_head = nullptr;
};

#endif
