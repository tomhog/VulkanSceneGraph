/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/raytracing/AccelerationStructureBuildTraversal.h>

using namespace vsg;

AccelerationStructureBuildTraversal::AccelerationStructureBuildTraversal(Device* in_device) :
    Visitor(),
    _device(in_device),
    _nextInstanceID(0)
{
    _tlas = TopLevelAccelerationStructure::create(_device);
}

void AccelerationStructureBuildTraversal::apply(Object& object)
{
    object.traverse(*this);
}

void AccelerationStructureBuildTraversal::apply(MatrixTransform& mt)
{
    _transformStack.pushAndPreMult(mt.getMatrix());

    mt.traverse(*this);

    _transformStack.pop();
}

void AccelerationStructureBuildTraversal::apply(vsg::Geometry& geometry)
{
    if (geometry._arrays.size() == 0) return;

    // check cache
    ref_ptr<BottomLevelAccelerationStructure> blas;
    if (_geometryBlasMap.find(&geometry) != _geometryBlasMap.end())
    {
        blas = _geometryBlasMap[&geometry];
    }
    else
    {
        // create new blas and add to cache
        blas = BottomLevelAccelerationStructure::create(_device);
        blas->_instanceId = _nextInstanceID;
        ref_ptr<AccelerationGeometry> accelGeom = AccelerationGeometry::create();
        accelGeom->_verts = geometry._arrays[0];
        accelGeom->_indices = geometry._indices;
        blas->_geometries.push_back(accelGeom);

        _geometryBlasMap[&geometry] = blas;
        addMeshBufferData(geometry._indices, geometry._arrays);

        _nextInstanceID++;
    }

    // create a geometry instance for this geometry using the blas that represents it and the current transform matrix
    createGeometryInstance(blas);
}

void AccelerationStructureBuildTraversal::apply(vsg::VertexIndexDraw& vid)
{
    if (vid._arrays.size() == 0) return;

    // check cache
    ref_ptr<BottomLevelAccelerationStructure> blas;
    if (_vertexIndexDrawBlasMap.find(&vid) != _vertexIndexDrawBlasMap.end())
    {
        blas = _vertexIndexDrawBlasMap[&vid];
    }
    else
    {
        // create new blas and add to cache
        blas = BottomLevelAccelerationStructure::create(_device);
        blas->_instanceId = _nextInstanceID;
        ref_ptr<AccelerationGeometry> accelGeom = AccelerationGeometry::create();
        accelGeom->_verts = vid._arrays[0];
        accelGeom->_indices = vid._indices;
        blas->_geometries.push_back(accelGeom);

        _vertexIndexDrawBlasMap[&vid] = blas;
        addMeshBufferData(vid._indices, vid._arrays);

        _nextInstanceID++;
    }

    // create a geometry instance for this geometry using the blas that represents it and the current transform matrix
    createGeometryInstance(blas);
}

void AccelerationStructureBuildTraversal::createGeometryInstance(BottomLevelAccelerationStructure* blas)
{
    ref_ptr<GeometryInstance> geominst = GeometryInstance::create();
    geominst->_accelerationStructure = blas;
    geominst->_id = blas->_instanceId;

    geominst->_transform = _transformStack.top();

    _tlas->_geometryInstances.push_back(geominst);
}

void AccelerationStructureBuildTraversal::addMeshBufferData(Data* indicies, const DataList& arrays)
{
    uint32_t triangleCount = static_cast<uint32_t>(indicies->valueCount() / 3);
    ref_ptr<uivec4Array> faces = uivec4Array::create(triangleCount);
    ref_ptr<uintArray> matids = uintArray::create(triangleCount);

    VkIndexType indexType = computeIndexType(indicies);
    ushortArray* usIndexArray = dynamic_cast<ushortArray*>(indicies);
    uintArray* uiIndexArray = dynamic_cast<uintArray*>(indicies);

    for (uint32_t i = 0; i < triangleCount; i++)
    {
        uint32_t a;
        uint32_t b;
        uint32_t c;

        if (indexType == VK_INDEX_TYPE_UINT16)
        {
            a = static_cast<uint32_t>(usIndexArray->at(i * 3));
            b = static_cast<uint32_t>(usIndexArray->at(a + 1));
            c =static_cast<uint32_t>( usIndexArray->at(a + 2));
        }
        else
        {
            a = uiIndexArray->at(i * 3);
            b = uiIndexArray->at(a + 1);
            c = uiIndexArray->at(a + 2);
        }

        faces->set(i, {a, b, c, 0});

        matids->set(i, 0); // need an index into an array of material/image descriptors
    }

    _faces.push_back(faces);

    // add mesh attributes (need better way to determine these)
    ref_ptr<MeshAttributeArray> meshatts = MeshAttributeArray::create(static_cast<uint32_t>(arrays[0]->valueCount()));

    // assume 0 exists and is verts
    vec3Array* verts = dynamic_cast<vec3Array*>(arrays[0].get());

    // assume 1 is normals if it's a vec3
    vec3Array* normals = dynamic_cast<vec3Array*>(arrays[1].get());

    // assume 2 is texcoord0 if it's a vec2
    vec2Array* texCoords0 = dynamic_cast<vec2Array*>(arrays[2].get());

    for (uint32_t i = 0; i < verts->valueCount(); i++)
    {
        vec3 normal = normals != nullptr ? normals->at(i) : vec3(0.0,1.0,0.0);
        vec2 tex0 = texCoords0 != nullptr ? texCoords0->at(i) : vec2(0.0, 0.0);
        meshatts->set(i, {normal, tex0});
    }

    _meshAttributes.push_back(meshatts);
}

void AccelerationStructureBuildTraversal::createMeshBufferDescriptors(uint32_t attributeIndex, uint32_t facesIndex, uint32_t matIdsIndex)
{
    _meshAttributesDescriptor = DescriptorBuffer::create(_meshAttributes, attributeIndex, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    _facesDescriptor = DescriptorBuffer::create(_faces, facesIndex, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    _matIdsDescriptor = DescriptorBuffer::create(_matIds, matIdsIndex, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
}
