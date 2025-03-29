//
// Created by Kolane on 2025/3/29.
//

#include <iostream>
#include <vector>

// 访问者模式中的元素接口
class Shape {
public:
    virtual ~Shape() = default;
    virtual void accept(class Visitor& v) = 0;  // 接受访问者的接口
};

// 具体元素类：Circle
class Circle : public Shape {
public:
    void accept(Visitor& v) override;
    double getRadius() const { return 5.0; }
};

// 具体元素类：Rectangle
class Rectangle : public Shape {
public:
    void accept(Visitor& v) override;
    double getWidth() const { return 4.0; }
    double getHeight() const { return 6.0; }
};

// 访问者接口
class Visitor {
public:
    virtual void visit(Circle& circle) = 0;   // 访问 Circle 元素
    virtual void visit(Rectangle& rectangle) = 0; // 访问 Rectangle 元素
};

// 具体访问者类：计算面积
class AreaCalculator : public Visitor {
public:
    void visit(Circle& circle) override {
        double area = 3.14159 * circle.getRadius() * circle.getRadius();
        std::cout << "Circle area: " << area << std::endl;
    }

    void visit(Rectangle& rectangle) override {
        double area = rectangle.getWidth() * rectangle.getHeight();
        std::cout << "Rectangle area: " << area << std::endl;
    }
};

// 具体访问者类：绘制形状
class DrawingVisitor : public Visitor {
public:
    void visit(Circle& circle) override {
        std::cout << "Drawing a circle with radius: " << circle.getRadius() << std::endl;
    }

    void visit(Rectangle& rectangle) override {
        std::cout << "Drawing a rectangle with width: " << rectangle.getWidth()
                  << " and height: " << rectangle.getHeight() << std::endl;
    }
};

void Circle::accept(Visitor& v) {
    v.visit(*this);  // 将当前对象传递给访问者
}

void Rectangle::accept(Visitor& v) {
    v.visit(*this);  // 将当前对象传递给访问者
}

int main() {
    std::vector<Shape*> shapes;  // 对象结构，可以存储多个形状
    shapes.push_back(new Circle());
    shapes.push_back(new Rectangle());

    // 创建访问者对象
    AreaCalculator areaCalculator;
    DrawingVisitor drawingVisitor;

    // 使用不同的访问者进行操作
    std::cout << "Calculating areas:" << std::endl;
    for (Shape* shape : shapes) {
        shape->accept(areaCalculator);  // 计算面积
    }

    std::cout << "\nDrawing shapes:" << std::endl;
    for (Shape* shape : shapes) {
        shape->accept(drawingVisitor);  // 绘制形状
    }

    // 清理资源
    for (Shape* shape : shapes) {
        delete shape;
    }

    return 0;
}
