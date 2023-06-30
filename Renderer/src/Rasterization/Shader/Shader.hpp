#pragma once

namespace soft {

    class Shader {
    public:
        Shader() = default;
        virtual void Main() {}

    private:
    };

    class VertexShader : public Shader {
    public:
        virtual void Main() override {}
    };

    class FragmentShader : public Shader {
    public:
        virtual void Main() override {}
    };

}  // namespace soft