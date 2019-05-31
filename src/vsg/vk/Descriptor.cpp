/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/traversals/CompileTraversal.h>
#include <vsg/vk/CommandBuffer.h>
#include <vsg/vk/Descriptor.h>

#include <iostream>

using namespace vsg;

/////////////////////////////////////////////////////////////////////////////////////////
//
// DescriptorBuffer
//
void DescriptorBuffer::copyDataListToBuffers()
{
    vsg::copyDataListToBuffers(_bufferDataList);
}

/////////////////////////////////////////////////////////////////////////////////////////
//
// Texture
//
Texture::Texture() :
    Inherit(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
{
    // set default sampler info
    _samplerInfo = {};
    _samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    _samplerInfo.minFilter = VK_FILTER_LINEAR;
    _samplerInfo.magFilter = VK_FILTER_LINEAR;
    _samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    _samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    _samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
#if 1
    // requires Logical device to have deviceFeatures.samplerAnisotropy = VK_TRUE; set when creating the vsg::Device
    _samplerInfo.anisotropyEnable = VK_TRUE;
    _samplerInfo.maxAnisotropy = 16;
#else
    _samplerInfo.anisotropyEnable = VK_FALSE;
    _samplerInfo.maxAnisotropy = 1;
#endif
    _samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    _samplerInfo.unnormalizedCoordinates = VK_FALSE;
    _samplerInfo.compareEnable = VK_FALSE;
    _samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

void Texture::read(Input& input)
{
    Descriptor::read(input);
#if 1
    input.readValue<uint32_t>("flags", _samplerInfo.flags);
    input.readValue<uint32_t>("minFilter", _samplerInfo.minFilter);
    input.readValue<uint32_t>("magFilter", _samplerInfo.magFilter);
    input.readValue<uint32_t>("mipmapMode", _samplerInfo.mipmapMode);
    input.readValue<uint32_t>("addressModeU", _samplerInfo.addressModeU);
    input.readValue<uint32_t>("addressModeV", _samplerInfo.addressModeV);
    input.readValue<uint32_t>("addressModeW", _samplerInfo.addressModeW);
    input.read("mipLodBias", _samplerInfo.mipLodBias);
    input.readValue<uint32_t>("anisotropyEnable", _samplerInfo.anisotropyEnable);
    input.read("maxAnisotropy", _samplerInfo.maxAnisotropy);
    input.readValue<uint32_t>("compareEnable", _samplerInfo.compareEnable);
    input.readValue<uint32_t>("compareOp", _samplerInfo.compareOp);
    input.read("minLod", _samplerInfo.minLod);
    input.read("maxLod", _samplerInfo.maxLod);
    input.readValue<uint32_t>("borderColor", _samplerInfo.borderColor);
    input.readValue<uint32_t>("unnormalizedCoordinates", _samplerInfo.unnormalizedCoordinates);
#endif
    _textureData = input.readObject<Data>("TextureData");
}

void Texture::write(Output& output) const
{
    Descriptor::write(output);
#if 1
    output.writeValue<uint32_t>("flags", _samplerInfo.flags);
    output.writeValue<uint32_t>("minFilter", _samplerInfo.minFilter);
    output.writeValue<uint32_t>("magFilter", _samplerInfo.magFilter);
    output.writeValue<uint32_t>("mipmapMode", _samplerInfo.mipmapMode);
    output.writeValue<uint32_t>("addressModeU", _samplerInfo.addressModeU);
    output.writeValue<uint32_t>("addressModeV", _samplerInfo.addressModeV);
    output.writeValue<uint32_t>("addressModeW", _samplerInfo.addressModeW);
    output.write("mipLodBias", _samplerInfo.mipLodBias);
    output.writeValue<uint32_t>("anisotropyEnable", _samplerInfo.anisotropyEnable);
    output.write("maxAnisotropy", _samplerInfo.maxAnisotropy);
    output.writeValue<uint32_t>("compareEnable", _samplerInfo.compareEnable);
    output.writeValue<uint32_t>("compareOp", _samplerInfo.compareOp);
    output.write("minLod", _samplerInfo.minLod);
    output.write("maxLod", _samplerInfo.maxLod);
    output.writeValue<uint32_t>("borderColor", _samplerInfo.borderColor);
    output.writeValue<uint32_t>("unnormalizedCoordinates", _samplerInfo.unnormalizedCoordinates);
#endif
    output.writeObject("TextureData", _textureData.get());
}

void Texture::compile(Context& context)
{
    if (_implementation) return;

    ref_ptr<Sampler> sampler = Sampler::create(context.device, _samplerInfo, nullptr);
    vsg::ImageData imageData = vsg::transferImageData(context, _textureData, sampler);
    if (!imageData.valid())
    {
        return;
    }

    _implementation = vsg::DescriptorImage::create(_dstBinding, _dstArrayElement, _descriptorType, vsg::ImageDataList{imageData});
}

bool Texture::assignTo(VkWriteDescriptorSet& wds, VkDescriptorSet descriptorSet) const
{
    if (_implementation)
        return _implementation->assignTo(wds, descriptorSet);
    else
        return false;
}

/////////////////////////////////////////////////////////////////////////////////////////
//
// TextureArray
//
TextureArray::TextureArray() :
    Inherit(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
{
}

void TextureArray::read(Input& input)
{
    Descriptor::read(input);

    _textures.resize(input.readValue<uint32_t>("NumTextures"));
    for (auto& texture : _textures)
    {
        texture = input.readObject<Texture>("Texture");
    }
}

void TextureArray::write(Output& output) const
{
    Descriptor::write(output);

    output.writeValue<uint32_t>("NumTextures", _textures.size());
    for (auto& texture : _textures)
    {
        output.writeObject("Texture", texture.get());
    }
}

void TextureArray::compile(Context& context)
{
    if (_implementation) return;

    vsg::ImageDataList images;
    for (const auto& texture : _textures)
    {
        ref_ptr<Sampler> sampler = Sampler::create(context.device, texture->_samplerInfo, nullptr);
        vsg::ImageData imageData = vsg::transferImageData(context, texture->_textureData, sampler);
        if (!imageData.valid())
        {
            return;
        }
        images.push_back(imageData);
    }

    _implementation = vsg::DescriptorImage::create(_dstBinding, _dstArrayElement, _descriptorType, images);
}

bool TextureArray::assignTo(VkWriteDescriptorSet& wds, VkDescriptorSet descriptorSet) const
{
    if (_implementation)
        return _implementation->assignTo(wds, descriptorSet);
    else
        return false;
}

/////////////////////////////////////////////////////////////////////////////////////////
//
// Uniform
//
Uniform::Uniform() :
    Inherit(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
{
}

void Uniform::read(Input& input)
{
    Descriptor::read(input);

    _dataList.resize(input.readValue<uint32_t>("NumData"));
    for (auto& data : _dataList)
    {
        data = input.readObject<Data>("Data");
    }
}

void Uniform::write(Output& output) const
{
    Descriptor::write(output);

    output.writeValue<uint32_t>("NumData", _dataList.size());
    for (auto& data : _dataList)
    {
        output.writeObject("Data", data.get());
    }
}

void Uniform::compile(Context& context)
{
    if (_implementation) return;

    auto bufferDataList = vsg::createHostVisibleBuffer(context.device, _dataList, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
    vsg::copyDataListToBuffers(bufferDataList);
    _implementation = vsg::DescriptorBuffer::create(_dstBinding, _dstArrayElement, _descriptorType, bufferDataList);
}

bool Uniform::assignTo(VkWriteDescriptorSet& wds, VkDescriptorSet descriptorSet) const
{
    if (_implementation)
        return _implementation->assignTo(wds, descriptorSet);
    else
        return false;
}

void Uniform::copyDataListToBuffers()
{
    if (_implementation) _implementation->copyDataListToBuffers();
}
