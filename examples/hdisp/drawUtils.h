#ifndef DRAWUTILS_H
#define DRAWUTILS_H

class DrawUtils
{
public:
    bool Init();

    void SetModelViewProjection(const float *mvp);
    void Bind();
    void Unbind();

};

#endif // DRAWUTILS_H
