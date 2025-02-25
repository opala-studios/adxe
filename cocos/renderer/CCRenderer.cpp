/****************************************************************************
 Copyright (c) 2013-2016 Chukong Technologies Inc.
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
 Copyright (c) 2020 c4games.com.

 http://www.cocos2d-x.org

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/
#include "renderer/CCRenderer.h"

#include <algorithm>

#include "renderer/CCTrianglesCommand.h"
#include "renderer/CCCustomCommand.h"
#include "renderer/CCCallbackCommand.h"
#include "renderer/CCGroupCommand.h"
#include "renderer/CCMeshCommand.h"
#include "renderer/CCMaterial.h"
#include "renderer/CCTechnique.h"
#include "renderer/CCPass.h"
#include "renderer/CCTexture2D.h"

#include "base/CCConfiguration.h"
#include "base/CCDirector.h"
#include "base/CCEventDispatcher.h"
#include "base/CCEventListenerCustom.h"
#include "base/CCEventType.h"
#include "2d/CCCamera.h"
#include "2d/CCScene.h"
#include "xxhash.h"

#include "renderer/backend/Backend.h"
#include "renderer/backend/RenderTarget.h"

NS_CC_BEGIN

// helper
static bool compareRenderCommand(RenderCommand* a, RenderCommand* b)
{
    return a->getGlobalOrder() < b->getGlobalOrder();
}

static bool compare3DCommand(RenderCommand* a, RenderCommand* b)
{
    return  a->getDepth() > b->getDepth();
}

// queue
RenderQueue::RenderQueue()
{
}

void RenderQueue::push_back(RenderCommand* command)
{
    float z = command->getGlobalOrder();
    if(z < 0)
    {
        _commands[QUEUE_GROUP::GLOBALZ_NEG].push_back(command);
    }
    else if(z > 0)
    {
        _commands[QUEUE_GROUP::GLOBALZ_POS].push_back(command);
    }
    else
    {
        if(command->is3D())
        {
            if(command->isTransparent())
            {
                _commands[QUEUE_GROUP::TRANSPARENT_3D].push_back(command);
            }
            else
            {
                _commands[QUEUE_GROUP::OPAQUE_3D].push_back(command);
            }
        }
        else
        {
            _commands[QUEUE_GROUP::GLOBALZ_ZERO].push_back(command);
        }
    }
}

ssize_t RenderQueue::size() const
{
    ssize_t result(0);
    for(int index = 0; index < QUEUE_GROUP::QUEUE_COUNT; ++index)
    {
        result += _commands[index].size();
    }
    
    return result;
}

void RenderQueue::sort()
{
    // Don't sort _queue0, it already comes sorted
    std::stable_sort(std::begin(_commands[QUEUE_GROUP::TRANSPARENT_3D]), std::end(_commands[QUEUE_GROUP::TRANSPARENT_3D]), compare3DCommand);
    std::stable_sort(std::begin(_commands[QUEUE_GROUP::GLOBALZ_NEG]), std::end(_commands[QUEUE_GROUP::GLOBALZ_NEG]), compareRenderCommand);
    std::stable_sort(std::begin(_commands[QUEUE_GROUP::GLOBALZ_POS]), std::end(_commands[QUEUE_GROUP::GLOBALZ_POS]), compareRenderCommand);
}

RenderCommand* RenderQueue::operator[](ssize_t index) const
{
    for(int queIndex = 0; queIndex < QUEUE_GROUP::QUEUE_COUNT; ++queIndex)
    {
        if(index < static_cast<ssize_t>(_commands[queIndex].size()))
            return _commands[queIndex][index];
        else
        {
            index -= _commands[queIndex].size();
        }
    }
    
    CCASSERT(false, "invalid index");
    return nullptr;
}

void RenderQueue::clear()
{
    for(int i = 0; i < QUEUE_GROUP::QUEUE_COUNT; ++i)
    {
        _commands[i].clear();
    }
}

void RenderQueue::realloc(size_t reserveSize)
{
    for(int i = 0; i < QUEUE_GROUP::QUEUE_COUNT; ++i)
    {
        _commands[i] = std::vector<RenderCommand*>();
        _commands[i].reserve(reserveSize);
    }
}

//
//
//
static const int DEFAULT_RENDER_QUEUE = 0;

//
// constructors, destructor, init
//
Renderer::Renderer()
{
    _groupCommandManager = new (std::nothrow) GroupCommandManager();
    
    _commandGroupStack.push(DEFAULT_RENDER_QUEUE);
    
    RenderQueue defaultRenderQueue;
    _renderGroups.push_back(defaultRenderQueue);
    _queuedTriangleCommands.reserve(BATCH_TRIAGCOMMAND_RESERVED_SIZE);

    // for the batched TriangleCommand
    _triBatchesToDraw = (TriBatchToDraw*) malloc(sizeof(_triBatchesToDraw[0]) * _triBatchesToDrawCapacity);
}

Renderer::~Renderer()
{
    _renderGroups.clear();
    _groupCommandManager->release();
    
    for(auto clearCommand : _clearCommandsPool)
        delete clearCommand;
    _clearCommandsPool.clear();
    
    free(_triBatchesToDraw);
    
    CC_SAFE_RELEASE(_depthStencilState);
    CC_SAFE_RELEASE(_commandBuffer);
    CC_SAFE_RELEASE(_renderPipeline);
    CC_SAFE_RELEASE(_defaultRT);
}

void Renderer::init()
{
    // Should invoke _triangleCommandBufferManager.init() first.
    _triangleCommandBufferManager.init();
    _vertexBuffer = _triangleCommandBufferManager.getVertexBuffer();
    _indexBuffer = _triangleCommandBufferManager.getIndexBuffer();

    auto device = backend::Device::getInstance();
    _commandBuffer = device->newCommandBuffer();
    // @MTL: the depth stencil flags must same render target and _dsDesc
    _dsDesc.flags = DepthStencilFlags::ALL;
    _defaultRT = device->newDefaultRenderTarget(TargetBufferFlags::COLOR | TargetBufferFlags::DEPTH_AND_STENCIL);
    
    _currentRT = _defaultRT;
    _renderPipeline = device->newRenderPipeline();
    _commandBuffer->setRenderPipeline(_renderPipeline);

    _depthStencilState = device->newDepthStencilState();
    _commandBuffer->setDepthStencilState(_depthStencilState);
}

void Renderer::addCommand(RenderCommand* command)
{
    int renderQueueID =_commandGroupStack.top();
    addCommand(command, renderQueueID);
}

void Renderer::addCommand(RenderCommand* command, int renderQueueID)
{
    CCASSERT(!_isRendering, "Cannot add command while rendering");
    CCASSERT(renderQueueID >=0, "Invalid render queue");
    CCASSERT(command->getType() != RenderCommand::Type::UNKNOWN_COMMAND, "Invalid Command Type");

    _renderGroups[renderQueueID].push_back(command);
}

void Renderer::pushGroup(int renderQueueID)
{
    CCASSERT(!_isRendering, "Cannot change render queue while rendering");
    _commandGroupStack.push(renderQueueID);
}

void Renderer::popGroup()
{
    CCASSERT(!_isRendering, "Cannot change render queue while rendering");
    _commandGroupStack.pop();
}

int Renderer::createRenderQueue()
{
    RenderQueue newRenderQueue;
    _renderGroups.push_back(newRenderQueue);
    return (int)_renderGroups.size() - 1;
}

void Renderer::processGroupCommand(GroupCommand* command)
{
    flush();

    int renderQueueID = ((GroupCommand*) command)->getRenderQueueID();

    pushStateBlock();
    //apply default state for all render queues
    setDepthTest(false);
    setDepthWrite(false);
    setCullMode(backend::CullMode::NONE);
    visitRenderQueue(_renderGroups[renderQueueID]);
    popStateBlock();
}

void Renderer::processRenderCommand(RenderCommand* command)
{
    auto commandType = command->getType();
    switch(commandType)
    {
        case RenderCommand::Type::TRIANGLES_COMMAND:
        {
            // flush other queues
            flush3D();
            
            auto cmd = static_cast<TrianglesCommand*>(command);
            
            // flush own queue when buffer is full
            if(_queuedTotalVertexCount + cmd->getVertexCount() > VBO_SIZE || _queuedTotalIndexCount + cmd->getIndexCount() > INDEX_VBO_SIZE)
            {
                CCASSERT(cmd->getVertexCount()>= 0 && cmd->getVertexCount() < VBO_SIZE, "VBO for vertex is not big enough, please break the data down or use customized render command");
                CCASSERT(cmd->getIndexCount()>= 0 && cmd->getIndexCount() < INDEX_VBO_SIZE, "VBO for index is not big enough, please break the data down or use customized render command");
                drawBatchedTriangles();

                _queuedTotalIndexCount = _queuedTotalVertexCount = 0;
#ifdef CC_USE_METAL
                _queuedIndexCount = _queuedVertexCount = 0;
                _triangleCommandBufferManager.prepareNextBuffer();
                _vertexBuffer = _triangleCommandBufferManager.getVertexBuffer();
                _indexBuffer = _triangleCommandBufferManager.getIndexBuffer();
#endif
            }
            
            // queue it
            _queuedTriangleCommands.push_back(cmd);
#ifdef CC_USE_METAL
            _queuedIndexCount += cmd->getIndexCount();
            _queuedVertexCount += cmd->getVertexCount();
#endif
            _queuedTotalVertexCount += cmd->getVertexCount();
            _queuedTotalIndexCount += cmd->getIndexCount();

        }
            break;
        case RenderCommand::Type::MESH_COMMAND:
            flush2D();
            drawMeshCommand(command);
            break;
        case RenderCommand::Type::GROUP_COMMAND:
            processGroupCommand(static_cast<GroupCommand*>(command));
            break;
        case RenderCommand::Type::CUSTOM_COMMAND:
            flush();
            drawCustomCommand(command);
            break;
        case RenderCommand::Type::CALLBACK_COMMAND:
            flush();
           static_cast<CallbackCommand*>(command)->execute();
            break;
        default:
            assert(false);
            break;
    }
}

void Renderer::visitRenderQueue(RenderQueue& queue)
{
    //
    //Process Global-Z < 0 Objects
    //
    doVisitRenderQueue(queue.getSubQueue(RenderQueue::QUEUE_GROUP::GLOBALZ_NEG));

    //
    //Process Opaque Object
    //
    pushStateBlock();
    setDepthTest(true); //enable depth test in 3D queue by default
    setDepthWrite(true);
    setCullMode(backend::CullMode::BACK);
    doVisitRenderQueue(queue.getSubQueue(RenderQueue::QUEUE_GROUP::OPAQUE_3D));
    
    //
    //Process 3D Transparent object
    //
    setDepthWrite(false);
    doVisitRenderQueue(queue.getSubQueue(RenderQueue::QUEUE_GROUP::TRANSPARENT_3D));
    popStateBlock();

    //
    //Process Global-Z = 0 Queue
    //
    doVisitRenderQueue(queue.getSubQueue(RenderQueue::QUEUE_GROUP::GLOBALZ_ZERO));
        
    //
    //Process Global-Z > 0 Queue
    //
    doVisitRenderQueue(queue.getSubQueue(RenderQueue::QUEUE_GROUP::GLOBALZ_POS));
}

void Renderer::doVisitRenderQueue(const std::vector<RenderCommand*>& renderCommands)
{
    for (const auto& command : renderCommands)
    {
        processRenderCommand(command);
    }
    flush();
}

void Renderer::render()
{
    //TODO: setup camera or MVP
    _isRendering = true;
//    if (_glViewAssigned)
    {
        //Process render commands
        //1. Sort render commands based on ID
        for (auto &renderqueue : _renderGroups)
        {
            renderqueue.sort();
        }
        visitRenderQueue(_renderGroups[0]);
    }
    clean();
    _isRendering = false;
}

bool Renderer::beginFrame()
{
    return _commandBuffer->beginFrame();
}

void Renderer::endFrame()
{
    _commandBuffer->endFrame();

#ifdef CC_USE_METAL
    _triangleCommandBufferManager.putbackAllBuffers();
    _vertexBuffer = _triangleCommandBufferManager.getVertexBuffer();
    _indexBuffer = _triangleCommandBufferManager.getIndexBuffer();
#endif
    _queuedTotalIndexCount = 0;
    _queuedTotalVertexCount = 0;
}

void Renderer::clean()
{
    // Clear render group
    for (size_t j = 0, size = _renderGroups.size() ; j < size; j++)
    {
        //commands are owned by nodes
        // for (const auto &cmd : _renderGroups[j])
        // {
        //     cmd->releaseToCommandPool();
        // }
        _renderGroups[j].clear();
    }

    // Clear batch commands
    _queuedTriangleCommands.clear();
}

void Renderer::setDepthTest(bool value)
{
    if (value) {
        _currentRT->addFlag(TargetBufferFlags::DEPTH);
        _dsDesc.addFlag(DepthStencilFlags::DEPTH_TEST);
    }
    else {
        _currentRT->removeFlag(TargetBufferFlags::DEPTH);
        _dsDesc.removeFlag(DepthStencilFlags::DEPTH_TEST);
    }
}

void Renderer::setStencilTest(bool value)
{
    if (value) {
        _currentRT->addFlag(TargetBufferFlags::STENCIL);
        _dsDesc.addFlag(DepthStencilFlags::STENCIL_TEST);
    }
    else {
        _currentRT->removeFlag(TargetBufferFlags::STENCIL);
        _dsDesc.removeFlag(DepthStencilFlags::STENCIL_TEST);
    }
}

void Renderer::setDepthWrite(bool value)
{
    if(value)
        _dsDesc.addFlag(DepthStencilFlags::DEPTH_WRITE);
    else
        _dsDesc.removeFlag(DepthStencilFlags::DEPTH_WRITE);
}

void Renderer::setDepthCompareFunction(backend::CompareFunction func)
{
    _dsDesc.depthCompareFunction = func;
}

backend::CompareFunction Renderer::getDepthCompareFunction() const
{
    return _dsDesc.depthCompareFunction;
}

bool Renderer::Renderer::getDepthTest() const
{
    return bitmask::any(_dsDesc.flags, DepthStencilFlags::DEPTH_TEST);
}

bool Renderer::getStencilTest() const
{
    return bitmask::any(_dsDesc.flags, DepthStencilFlags::STENCIL_TEST);
}

bool Renderer::getDepthWrite() const
{
    return bitmask::any(_dsDesc.flags, DepthStencilFlags::DEPTH_WRITE);
}

void Renderer::setStencilCompareFunction(backend::CompareFunction func, unsigned int ref, unsigned int readMask)
{
    _dsDesc.frontFaceStencil.stencilCompareFunction = func;
    _dsDesc.backFaceStencil.stencilCompareFunction = func;

    _dsDesc.frontFaceStencil.readMask = readMask;
    _dsDesc.backFaceStencil.readMask = readMask;

    _stencilRef = ref;
}

void Renderer::setStencilOperation(backend::StencilOperation stencilFailureOp,
                             backend::StencilOperation depthFailureOp,
                             backend::StencilOperation stencilDepthPassOp)
{
    _dsDesc.frontFaceStencil.stencilFailureOperation = stencilFailureOp;
    _dsDesc.backFaceStencil.stencilFailureOperation = stencilFailureOp;

    _dsDesc.frontFaceStencil.depthFailureOperation = depthFailureOp;
    _dsDesc.backFaceStencil.depthFailureOperation = depthFailureOp;

    _dsDesc.frontFaceStencil.depthStencilPassOperation = stencilDepthPassOp;
    _dsDesc.backFaceStencil.depthStencilPassOperation = stencilDepthPassOp;
}

void Renderer::setStencilWriteMask(unsigned int mask)
{
    _dsDesc.frontFaceStencil.writeMask = mask;
    _dsDesc.backFaceStencil.writeMask = mask;
}

backend::StencilOperation Renderer::getStencilFailureOperation() const
{
    return _dsDesc.frontFaceStencil.stencilFailureOperation;
}

backend::StencilOperation Renderer::getStencilPassDepthFailureOperation() const
{
    return _dsDesc.frontFaceStencil.depthFailureOperation;
}

backend::StencilOperation Renderer::getStencilDepthPassOperation() const
{
    return _dsDesc.frontFaceStencil.depthStencilPassOperation;
}

backend::CompareFunction Renderer::getStencilCompareFunction() const
{
    return _dsDesc.depthCompareFunction;
}

unsigned int Renderer::getStencilReadMask() const
{
    return _dsDesc.frontFaceStencil.readMask;
}

unsigned int Renderer::getStencilWriteMask() const
{
    return _dsDesc.frontFaceStencil.writeMask;
}

unsigned int Renderer::getStencilReferenceValue() const
{
    return _stencilRef;
}

void Renderer::setDepthStencilDesc(const backend::DepthStencilDescriptor& dsDesc)
{
    _dsDesc = dsDesc;
}

const backend::DepthStencilDescriptor& Renderer::getDepthStencilDesc() const
{
    return _dsDesc;
}

void Renderer::setViewPort(int x, int y, unsigned int w, unsigned int h)
{
    _viewport.x = x;
    _viewport.y = y;
    _viewport.w = w;
    _viewport.h = h;
}

void Renderer::fillVerticesAndIndices(const TrianglesCommand* cmd, unsigned int vertexBufferOffset)
{
    size_t vertexCount = cmd->getVertexCount();
    memcpy(&_verts[_filledVertex], cmd->getVertices(), sizeof(V3F_C4B_T2F) * vertexCount);
    
    // fill vertex, and convert them to world coordinates
    const Mat4& modelView = cmd->getModelView();
    for (size_t i=0; i < vertexCount; ++i)
    {
        modelView.transformPoint(&(_verts[i + _filledVertex].vertices));
    }
    
    // fill index
    const unsigned short* indices = cmd->getIndices();
    size_t indexCount = cmd->getIndexCount();
    for (size_t i = 0; i < indexCount; ++i)
    {
        _indices[_filledIndex + i] = vertexBufferOffset + _filledVertex + indices[i];
    }
    
    _filledVertex += vertexCount;
    _filledIndex += indexCount;
}

void Renderer::drawBatchedTriangles()
{
    if(_queuedTriangleCommands.empty())
        return;
    
    /************** 1: Setup up vertices/indices *************/
#ifdef CC_USE_METAL
    unsigned int vertexBufferFillOffset = _queuedTotalVertexCount - _queuedVertexCount;
    unsigned int indexBufferFillOffset = _queuedTotalIndexCount - _queuedIndexCount;
#else
    unsigned int vertexBufferFillOffset = 0;
    unsigned int indexBufferFillOffset = 0;
#endif

    _triBatchesToDraw[0].offset = indexBufferFillOffset;
    _triBatchesToDraw[0].indicesToDraw = 0;
    _triBatchesToDraw[0].cmd = nullptr;
    
    int batchesTotal = 0;
    uint32_t prevMaterialID = 0;
    bool firstCommand = true;

    _filledVertex = 0;
    _filledIndex = 0;

    for(const auto& cmd : _queuedTriangleCommands)
    {
        auto currentMaterialID = cmd->getMaterialID();
        const bool batchable = !cmd->isSkipBatching();
        
        fillVerticesAndIndices(cmd, vertexBufferFillOffset);
        
        // in the same batch ?
        if (batchable && (prevMaterialID == currentMaterialID || firstCommand))
        {
            CC_ASSERT((firstCommand || _triBatchesToDraw[batchesTotal].cmd->getMaterialID() == cmd->getMaterialID()) && "argh... error in logic");
            _triBatchesToDraw[batchesTotal].indicesToDraw += cmd->getIndexCount();
            _triBatchesToDraw[batchesTotal].cmd = cmd;
        }
        else
        {
            // is this the first one?
            if (!firstCommand)
            {
                batchesTotal++;
                _triBatchesToDraw[batchesTotal].offset = _triBatchesToDraw[batchesTotal - 1].offset + _triBatchesToDraw[batchesTotal - 1].indicesToDraw;
            }

            _triBatchesToDraw[batchesTotal].cmd = cmd;
            _triBatchesToDraw[batchesTotal].indicesToDraw = (int) cmd->getIndexCount();
            
            // is this a single batch ? Prevent creating a batch group then
            if (!batchable)
                 currentMaterialID = 0;
        }
        
        // capacity full ?
        if (batchesTotal + 1 >= _triBatchesToDrawCapacity)
        {
            _triBatchesToDrawCapacity *= 1.4;
            _triBatchesToDraw = (TriBatchToDraw*) realloc(_triBatchesToDraw, sizeof(_triBatchesToDraw[0]) * _triBatchesToDrawCapacity);
        }
        
        prevMaterialID = currentMaterialID;
        firstCommand = false;
    }
    batchesTotal++;
#ifdef CC_USE_METAL
    _vertexBuffer->updateSubData(_verts, vertexBufferFillOffset * sizeof(_verts[0]), _filledVertex * sizeof(_verts[0]));
    _indexBuffer->updateSubData(_indices, indexBufferFillOffset * sizeof(_indices[0]), _filledIndex * sizeof(_indices[0]));
#else
    _vertexBuffer->updateData(_verts, _filledVertex * sizeof(_verts[0]));
    _indexBuffer->updateData(_indices,  _filledIndex * sizeof(_indices[0]));
#endif
    
    /************** 2: Draw *************/
    beginRenderPass();

    _commandBuffer->setVertexBuffer(_vertexBuffer);
    _commandBuffer->setIndexBuffer(_indexBuffer);
    
    for (int i = 0; i < batchesTotal; ++i)
    {
        auto& drawInfo = _triBatchesToDraw[i];
        _commandBuffer->updatePipelineState(_currentRT, drawInfo.cmd->getPipelineDescriptor());
        auto& pipelineDescriptor = drawInfo.cmd->getPipelineDescriptor();
        _commandBuffer->setProgramState(pipelineDescriptor.programState);
        _commandBuffer->drawElements(backend::PrimitiveType::TRIANGLE,
                                     backend::IndexFormat::U_SHORT,
                                     drawInfo.indicesToDraw,
                                     drawInfo.offset * sizeof(_indices[0]));
       

        _drawnBatches++;
        _drawnVertices += _triBatchesToDraw[i].indicesToDraw;
    }

	endRenderPass();


    /************** 3: Cleanup *************/
    _queuedTriangleCommands.clear();

#ifdef CC_USE_METAL
    _queuedIndexCount = 0;
    _queuedVertexCount = 0;
#endif
}

void Renderer::drawCustomCommand(RenderCommand *command)
{
    auto cmd = static_cast<CustomCommand*>(command);

    if (cmd->getBeforeCallback()) cmd->getBeforeCallback()();

    beginRenderPass();
    _commandBuffer->setVertexBuffer(cmd->getVertexBuffer());

    _commandBuffer->updatePipelineState(_currentRT, cmd->getPipelineDescriptor());
    _commandBuffer->setProgramState(cmd->getPipelineDescriptor().programState);
    
    auto drawType = cmd->getDrawType();
    _commandBuffer->setLineWidth(cmd->getLineWidth());
    if (CustomCommand::DrawType::ELEMENT == drawType)
    {
        _commandBuffer->setIndexBuffer(cmd->getIndexBuffer());
        _commandBuffer->drawElements(cmd->getPrimitiveType(),
                                     cmd->getIndexFormat(),
                                     cmd->getIndexDrawCount(),
                                     cmd->getIndexDrawOffset());
        _drawnVertices += cmd->getIndexDrawCount();
    }
    else
    {
        _commandBuffer->drawArrays(cmd->getPrimitiveType(),
                                   cmd->getVertexDrawStart(),
                                   cmd->getVertexDrawCount());
        _drawnVertices += cmd->getVertexDrawCount();
    }
    _drawnBatches++;
    endRenderPass();

    if (cmd->getAfterCallback()) cmd->getAfterCallback()();
}

void Renderer::drawMeshCommand(RenderCommand *command)
{
    //MeshCommand and CustomCommand are identical while rendering.
    drawCustomCommand(command);
}


void Renderer::flush()
{
    flush2D();
    flush3D();
}

void Renderer::flush2D()
{
    flushTriangles();
}

void Renderer::flush3D()
{
    //TODO 3d batch rendering
}

void Renderer::flushTriangles()
{
    drawBatchedTriangles();
}

// helpers
bool Renderer::checkVisibility(const Mat4 &transform, const Size &size)
{
    auto director = Director::getInstance();
    auto scene = director->getRunningScene();

    //If draw to Rendertexture, return true directly.
    // only cull the default camera. The culling algorithm is valid for default camera.
    if (!scene || (scene && scene->_defaultCamera != Camera::getVisitingCamera()))
        return true;

    Rect visibleRect(director->getVisibleOrigin(), director->getVisibleSize());

    // transform center point to screen space
    float hSizeX = size.width/2;
    float hSizeY = size.height/2;
    Vec3 v3p(hSizeX, hSizeY, 0);
    transform.transformPoint(&v3p);
    Vec2 v2p = Camera::getVisitingCamera()->projectGL(v3p);

    // convert content size to world coordinates
    float wshw = std::max(fabsf(hSizeX * transform.m[0] + hSizeY * transform.m[4]), fabsf(hSizeX * transform.m[0] - hSizeY * transform.m[4]));
    float wshh = std::max(fabsf(hSizeX * transform.m[1] + hSizeY * transform.m[5]), fabsf(hSizeX * transform.m[1] - hSizeY * transform.m[5]));

    // enlarge visible rect half size in screen coord
    visibleRect.origin.x -= wshw;
    visibleRect.origin.y -= wshh;
    visibleRect.size.width += wshw * 2;
    visibleRect.size.height += wshh * 2;
    bool ret = visibleRect.containsPoint(v2p);
    return ret;
}

void Renderer::readPixels(backend::RenderTarget* rt, std::function<void(const backend::PixelBufferDescriptor&)> callback)
{
    assert(!!rt);
    if(rt == _defaultRT) // read pixels from screen, metal renderer backend: screen texture must not be a framebufferOnly
        backend::Device::getInstance()->setFrameBufferOnly(false);

    _commandBuffer->readPixels(rt, std::move(callback));
}

void Renderer::beginRenderPass()
{
    _commandBuffer->beginRenderPass(_currentRT, _renderPassDesc);
    _commandBuffer->updateDepthStencilState(_dsDesc);
    _commandBuffer->setStencilReferenceValue(_stencilRef);

    _commandBuffer->setViewport(_viewport.x, _viewport.y, _viewport.w, _viewport.h);
    _commandBuffer->setCullMode(_cullMode);
    _commandBuffer->setWinding(_winding);
    _commandBuffer->setScissorRect(_scissorState.isEnabled, _scissorState.rect.x, _scissorState.rect.y, _scissorState.rect.width, _scissorState.rect.height);
}

void Renderer::endRenderPass() 
{
    _commandBuffer->endRenderPass();
}

void Renderer::clear(ClearFlag flags, const Color4F& color, float depth, unsigned int stencil, float globalOrder)
{
    _clearFlag = flags;
    
    CallbackCommand* command = nextClearCommand();
    command->init(globalOrder);
    command->func = [=]() -> void {
        backend::RenderPassDescriptor descriptor;

        descriptor.flags.clear = flags;
        if (bitmask::any(flags, ClearFlag::COLOR)) {
            _clearColor = color;
            descriptor.clearColorValue = { color.r, color.g, color.b, color.a };
        }

        if(bitmask::any(flags, ClearFlag::DEPTH))
            descriptor.clearDepthValue = depth;

        if(bitmask::any(flags, ClearFlag::STENCIL))
            descriptor.clearStencilValue = stencil;
        
        _commandBuffer->beginRenderPass(_currentRT, descriptor);
        _commandBuffer->endRenderPass();

        // push to pool for reuse
        _clearCommandsPool.push_back(command);
    };
    addCommand(command);
}

CallbackCommand* Renderer::nextClearCommand()
{
    if(!_clearCommandsPool.empty()) {
        auto clearCommand = _clearCommandsPool.back();
        _clearCommandsPool.pop_back();
        return clearCommand;
    }
    
    return new CallbackCommand();
}

const Color4F& Renderer::getClearColor() const
{
    return _clearColor;
}

float Renderer::getClearDepth() const
{
    return _renderPassDesc.clearDepthValue;
}

unsigned int Renderer::getClearStencil() const
{
    return _renderPassDesc.clearStencilValue;
}

ClearFlag Renderer::getClearFlag() const
{
    return _clearFlag;
}

RenderTargetFlag Renderer::getRenderTargetFlag() const
{
    return _currentRT->getTargetFlags();
}

void Renderer::setScissorTest(bool enabled)
{
    _scissorState.isEnabled = enabled;
}

bool Renderer::getScissorTest() const
{
    return _scissorState.isEnabled;
}

const ScissorRect& Renderer::getScissorRect() const
{
    return _scissorState.rect;
}

void Renderer::setScissorRect(float x, float y, float width, float height)
{
    _scissorState.rect.x = x;
    _scissorState.rect.y = y;
    _scissorState.rect.width = width;
    _scissorState.rect.height = height;
}

// TriangleCommandBufferManager
Renderer::TriangleCommandBufferManager::~TriangleCommandBufferManager()
{
    for (auto& vertexBuffer : _vertexBufferPool)
        vertexBuffer->release();

    for (auto& indexBuffer : _indexBufferPool)
        indexBuffer->release();
}

void Renderer::TriangleCommandBufferManager::init()
{
    createBuffer();
}

void Renderer::TriangleCommandBufferManager::putbackAllBuffers()
{
    _currentBufferIndex = 0;
}

void Renderer::TriangleCommandBufferManager::prepareNextBuffer()
{
    if (_currentBufferIndex < (int)_vertexBufferPool.size() - 1)
    {
        ++_currentBufferIndex;
        return;
    }

    createBuffer();
    ++_currentBufferIndex;
}

backend::Buffer* Renderer::TriangleCommandBufferManager::getVertexBuffer() const
{
    return _vertexBufferPool[_currentBufferIndex];
}

backend::Buffer* Renderer::TriangleCommandBufferManager::getIndexBuffer() const
{
    return _indexBufferPool[_currentBufferIndex];
}

void Renderer::TriangleCommandBufferManager::createBuffer()
{
    auto device = backend::Device::getInstance();

#ifdef CC_USE_METAL
    // Metal doesn't need to update buffer to make sure it has the correct size.
    auto vertexBuffer = device->newBuffer(Renderer::VBO_SIZE * sizeof(_verts[0]), backend::BufferType::VERTEX, backend::BufferUsage::DYNAMIC);
    if (!vertexBuffer)
        return;

    auto indexBuffer = device->newBuffer(Renderer::INDEX_VBO_SIZE * sizeof(_indices[0]), backend::BufferType::INDEX, backend::BufferUsage::DYNAMIC);
    if (!indexBuffer)
    {
        vertexBuffer->release();
        return;
    }
#else
    auto tmpData = malloc(Renderer::VBO_SIZE * sizeof(V3F_C4B_T2F));
    if (!tmpData)
        return;

    auto vertexBuffer = device->newBuffer(Renderer::VBO_SIZE * sizeof(V3F_C4B_T2F), backend::BufferType::VERTEX, backend::BufferUsage::DYNAMIC);
    if (!vertexBuffer)
    {
        free(tmpData);
        return;
    }
    vertexBuffer->updateData(tmpData, Renderer::VBO_SIZE * sizeof(V3F_C4B_T2F));

    auto indexBuffer = device->newBuffer(Renderer::INDEX_VBO_SIZE * sizeof(unsigned short), backend::BufferType::INDEX, backend::BufferUsage::DYNAMIC);
    if (! indexBuffer)
    {
        free(tmpData);
        vertexBuffer->release();
        return;
    }
    indexBuffer->updateData(tmpData, Renderer::INDEX_VBO_SIZE * sizeof(unsigned short));

    free(tmpData);
#endif

    _vertexBufferPool.push_back(vertexBuffer);
    _indexBufferPool.push_back(indexBuffer);
}

void Renderer::pushStateBlock()
{
    StateBlock block;
    block.depthTest = getDepthTest();
    block.depthWrite = getDepthWrite();
    block.cullMode = getCullMode();
    _stateBlockStack.emplace_back(block);
}

void Renderer::popStateBlock()
{
    auto & block = _stateBlockStack.back();
    setDepthTest(block.depthTest);
    setDepthWrite(block.depthWrite);
    setCullMode(block.cullMode);
    _stateBlockStack.pop_back();
}

NS_CC_END
