/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdlib.h>
#include <cassert>


// Local includes
#include <nixl.h>
#include <common/nixl_time.h>
#include <serdes/serdes.h>
#include <ucx_mo_backend.h>

using namespace std;

/****************************************
 * CUDA related code
*****************************************/

#ifdef HAVE_CUDA

#include <cuda_runtime.h>
#include <cufile.h>



static uint32_t _getNumVramDevices()
{
    cudaError_t result;
    int n_vram_dev;
    result = cudaGetDeviceCount(&n_vram_dev);
    if (result != cudaSuccess) {
        return 0;
    } else {
        return n_vram_dev;
    }
}


#else

static uint32_t _getNumVramDevices(){
    return 0;
}

#endif

/****************************************
 * UCX Engine management
*****************************************/


int
nixlUcxMoEngine::setEngCnt(uint32_t num_host)
{
    _gpuCnt = _getNumVramDevices();
    _engineCnt = (_gpuCnt > num_host) ? _gpuCnt : num_host;
    return 0;
}

uint32_t
nixlUcxMoEngine::getEngCnt()
{
    return _engineCnt;
}

int32_t
nixlUcxMoEngine::getEngIdx(nixl_mem_t type, uint32_t devId)
{
    switch (type) {
    case VRAM_SEG:
        assert(devId < _gpuCnt);
        if (devId < _gpuCnt) {
            return -1;
        }
    case DRAM_SEG:
        break; 
    default:
        return -1;
    }
    assert(devId < _engineCnt);
    return (devId < _engineCnt) ? devId : -1;
}

string
nixlUcxMoEngine::getEngName(const string &baseName, uint32_t eidx)
{
    return baseName + ":" + to_string(eidx);
}

string
nixlUcxMoEngine::getEngBase(const string &engName)
{
    // find the last occurence (agent name may have colon in its name)
    if (string::npos == engName.find_last_of(":")) {
        assert(engName.find_last_of(":") != string::npos);
        return engName;
    }
    return engName.substr(0, engName.find_last_of(":"));
}

/****************************************
 * Constructor/Destructor
*****************************************/

nixlUcxMoEngine::nixlUcxMoEngine(const nixlBackendInitParams* init_params):
                                 nixlBackendEngine(init_params)
{
    nixl_b_params_t* custom_params = init_params->customParams;
    uint32_t num_ucx_engines = 1;
    if (custom_params->count("num_ucx_engines")) {
        const char *cptr = (*custom_params)["num_ucx_engines"].c_str();
        char *eptr;
        uint32_t tmp = strtoul(cptr, &eptr, 0);
        if ( (size_t)(eptr - cptr) == (*custom_params)["num_ucx_engines"].length()) {
            num_ucx_engines = tmp;
        } else {
            this->initErr = true;
            // TODO: Log error
            return;
        }
    }

    setEngCnt(num_ucx_engines);
    // Initialize required number of engines
    for (uint32_t i = 0; i < getEngCnt(); i++) {
        nixlBackendEngine *e;
        e = (nixlBackendEngine *)new nixlUcxEngine(init_params);
        engines.push_back(e);
        if (engines[0]->getInitErr()) {
            this->initErr = true;
            // TODO: Log error
            return;
        }
    }
}

nixl_status_t
nixlUcxMoEngine::getSupportedMems (std::vector<nixl_mem_t> &mems) const 
{
    mems.push_back(DRAM_SEG);
    mems.push_back(VRAM_SEG);
    return NIXL_SUCCESS;
}

nixlUcxMoEngine::~nixlUcxMoEngine()
{
    // TODO: won't need the access after rebase
    for( auto &e : engines ) {
        delete e;
    }
}

/****************************************
 * Connection management
*****************************************/

nixl_status_t
nixlUcxMoEngine::getConnInfo(std::string &str) const
{
    nixlSerDes sd;
    nixl_status_t status;

    // Serialize the number of engines
    size_t sz = engines.size();
    sd.addBuf("Count", &sz, sizeof(sz));

    for( auto &e : engines ) {
        string s;
        status = e->getConnInfo(s);
        if (NIXL_SUCCESS != status) {
            return status;
        }
        sd.addStr("Value", s);
    }

    str = sd.exportStr();
    return NIXL_SUCCESS;
}


nixl_status_t
nixlUcxMoEngine::loadRemoteConnInfo (const string  &remote_agent,
                                     const string &remote_conn_info)
{
    nixlSerDes sd;
    nixlUcxMoConnection conn;
    nixl_status_t status;
    size_t sz;
    remote_comm_it_t it = remoteConnMap.find(remote_agent);
    

    if(it != remoteConnMap.end()) {
        return NIXL_ERR_INVALID_PARAM;
    }

    conn.remoteAgent = remote_agent;

    status = sd.importStr(remote_conn_info);
    if (status != NIXL_SUCCESS) {
        return status;
    }

    ssize_t ret = sd.getBufLen("Count");
    if (ret != sizeof(sz)) {
        return NIXL_ERR_MISMATCH;
    }
    status = sd.getBuf("Count", &sz, ret);
    if (status != NIXL_SUCCESS) {
        return status;
    }

    conn.num_engines = sz;

    for(size_t idx = 0; idx < sz; idx++) {
        string cinfo;
        cinfo = sd.getStr("Value");
        for (auto &e : engines) {
            status = e->loadRemoteConnInfo(getEngName(remote_agent, idx), cinfo);
            if (status != NIXL_SUCCESS) {
                return status;
            }
        }
    }

    remoteConnMap[remote_agent] = conn;

    return NIXL_SUCCESS;
}

nixl_status_t
nixlUcxMoEngine::connect(const string &remote_agent)
{
    remote_comm_it_t it = remoteConnMap.find(remote_agent);
    nixl_status_t status;

    if(it == remoteConnMap.end()) {
        return NIXL_ERR_NOT_FOUND;
    }

    nixlUcxMoConnection &conn = it->second;

    for (auto &e : engines) {
        for (uint32_t idx = 0; idx < conn.num_engines; idx++) {
            status = e->connect(getEngName(remote_agent, idx));
            if (status != NIXL_SUCCESS) {
                return status;
            }
        }
    }

    return NIXL_SUCCESS;
}

nixl_status_t
nixlUcxMoEngine::disconnect(const string &remote_agent)
{
    nixl_status_t status;
    remote_comm_it_t it = remoteConnMap.find(remote_agent);

    if(it == remoteConnMap.end()) {
        return NIXL_ERR_NOT_FOUND;
    }

    nixlUcxMoConnection &conn = it->second;

    for (auto &e : engines) {
        for (uint32_t idx = 0; idx < conn.num_engines; idx++) {
            status = e->disconnect(getEngName(remote_agent, idx));
            if (status != NIXL_SUCCESS) {
                return status;
            }
        }
    }

    remoteConnMap.erase(remote_agent);

    return NIXL_SUCCESS;
}

/****************************************
 * Memory management
*****************************************/


nixl_status_t
nixlUcxMoEngine::registerMem (const nixlBlobDesc &mem,
                              const nixl_mem_t &nixl_mem,
                              nixlBackendMD* &out)
{
    nixlUcxMoPrivateMetadata *priv = new nixlUcxMoPrivateMetadata;
    int32_t eidx = getEngIdx(nixl_mem, mem.devId);
    nixlSerDes sd;
    string str;
    nixl_status_t status;

    if (eidx < 0) {
        return NIXL_ERR_INVALID_PARAM;
    }

    priv->eidx = eidx;
    engines[eidx]->registerMem(mem, nixl_mem, priv->md);

    sd.addBuf("EngIdx", &eidx, sizeof(eidx));
    status = engines[eidx]->getPublicData(priv->md, str);
    if (NIXL_SUCCESS != status) {
        return status;
    }
    sd.addStr("RkeyStr", str);
    priv->rkeyStr = sd.exportStr();
    out = (nixlBackendMD*) priv;

    return NIXL_SUCCESS;
}

nixl_status_t 
nixlUcxMoEngine::getPublicData (const nixlBackendMD* meta,
                              std::string &str) const
{
    const nixlUcxMoPrivateMetadata *priv = (nixlUcxMoPrivateMetadata*) meta;
    str = priv->get();
    return NIXL_SUCCESS;
}

nixl_status_t 
nixlUcxMoEngine::deregisterMem (nixlBackendMD* meta)
{
    nixlUcxMoPrivateMetadata *priv = (nixlUcxMoPrivateMetadata*) meta;

    engines[priv->eidx]->deregisterMem(priv->md);
    delete priv;
    return NIXL_SUCCESS;
}

nixl_status_t
nixlUcxMoEngine::loadLocalMD(nixlBackendMD* input,
                             nixlBackendMD* &output)
{
    // TODO
    return NIXL_ERR_NOT_FOUND;
}

nixl_status_t
nixlUcxMoEngine::loadRemoteMD (const nixlBlobDesc &input,
                               const nixl_mem_t &nixl_mem,
                               const string &remote_agent,
                               nixlBackendMD* &output)
{
    nixlUcxMoConnection conn;
    nixlSerDes sd;
    string rkeyStr;
    nixl_status_t status;
    nixlBlobDesc input_int;

    nixlUcxMoPublicMetadata *md = new nixlUcxMoPublicMetadata;

    auto search = remoteConnMap.find(remote_agent);

    if(search == remoteConnMap.end()) {
        //TODO: err: remote connection not found
        return NIXL_ERR_NOT_FOUND;
    }
    conn = (nixlUcxMoConnection) search->second;

    status = sd.importStr(input.metaInfo);

    ssize_t ret = sd.getBufLen("EngIdx");
    if (ret != sizeof(md->eidx)) {
        return NIXL_ERR_MISMATCH;
    }
    status = sd.getBuf("EngIdx", &md->eidx, ret);
    if (status != NIXL_SUCCESS) {
        return status;
    }

    rkeyStr = sd.getStr("RkeyStr");
    if (status != NIXL_SUCCESS) {
        return status;
    }

    for (auto &e : engines) {
        nixlBackendMD *int_md;
        input_int.metaInfo = rkeyStr;
        status = e->loadRemoteMD(input_int, nixl_mem, 
                                 getEngName(remote_agent, md->eidx),
                                 int_md);
        if (status != NIXL_SUCCESS) {
            return status;
        }
        md->int_mds.push_back(int_md);
    }

    output = (nixlBackendMD*)md;
    return NIXL_SUCCESS;
}

nixl_status_t
nixlUcxMoEngine::unloadMD (nixlBackendMD* input)
{
    nixl_status_t status;

    nixlUcxMoPublicMetadata *md = (nixlUcxMoPublicMetadata *)input;
    for (size_t i = 0; i < md->int_mds.size(); i++) {
        status = engines[i]->unloadMD(md->int_mds[i]);
        if (NIXL_SUCCESS != status) {
            return status;
        }
    }
    return NIXL_SUCCESS;
}

/****************************************
 * Data movement
*****************************************/

void 
nixlUcxMoEngine::cancelRequests(nixlUcxMoRequestH *req)
{
    // Iterate over all elements cancelling each one
    for ( auto &p : req->reqs ) {
        p.first->releaseReqH(p.second);
        p.first = NULL;
        p.second = NULL;
    }

}


nixl_status_t
nixlUcxMoEngine::retHelper(nixl_status_t ret, nixlBackendEngine *eng,
                           nixlUcxMoRequestH *req, nixlBackendReqH *&int_req)
{
    /* if transfer wasn't immediately completed */
    switch(ret) {
    case NIXL_IN_PROG:
        req->reqs.push_back(nixlUcxMoRequestH::req_pair_t{eng, int_req});
    case NIXL_SUCCESS:
        // Nothing to do
        return NIXL_SUCCESS;
    default:
        // Error. Release all previously initiated ops and exit:
        cancelRequests(req);
        delete(req);
        return ret;
    }
}

nixl_status_t
nixlUcxMoEngine::prepXfer (const nixl_xfer_op_t &operation,
                           const nixl_meta_dlist_t &local,
                           const nixl_meta_dlist_t &remote,
                           const std::string &remote_agent,
                           nixlBackendReqH* &handle,
                           const nixl_opt_b_args_t *opt_args)
{
    return NIXL_SUCCESS;
}


// Data transfer
nixl_status_t 
nixlUcxMoEngine::postXfer (const nixl_xfer_op_t &op,
                           const nixl_meta_dlist_t &local,
                           const nixl_meta_dlist_t &remote,
                           const std::string &remote_agent,
                           nixlBackendReqH* &out_handle,
                           const nixl_opt_b_args_t *opt_args)
{
    size_t l_eng_cnt = engines.size();
    size_t r_eng_cnt;
    int des_cnt = local.descCount();
    nixlUcxMoRequestH *req = new nixlUcxMoRequestH;
    nixl_xfer_op_t op_int;
    remote_comm_it_t it = remoteConnMap.find(remote_agent);

    // Input check
    if (des_cnt != remote.descCount()) {
        return NIXL_ERR_INVALID_PARAM;
    }

    if(it == remoteConnMap.end()) {
        return NIXL_ERR_INVALID_PARAM;
    }
    nixlUcxMoConnection &conn = it->second;

    /* Allocate temp distribution matrix */
    r_eng_cnt = conn.num_engines;
    typedef pair<nixl_meta_dlist_t *,nixl_meta_dlist_t *> _dlist_pair_t;
    typedef vector<vector<_dlist_pair_t>> _dlist_matrix_t;
    _dlist_matrix_t dlmatrix(l_eng_cnt, vector<_dlist_pair_t>(r_eng_cnt, _dlist_pair_t{NULL, NULL}));


    // Convert the operation type
    switch(op) {
    case NIXL_READ:
    case NIXL_RD_NOTIF:
        op_int = NIXL_READ;
        break;
    case NIXL_WRITE:
    case NIXL_WR_NOTIF:
        op_int = NIXL_WRITE;
        break;
    default:
        return NIXL_ERR_INVALID_PARAM;
    }

    /* Go over all input */
    for(int i = 0; i < des_cnt; i++) {
        size_t lsize = local[i].len;
        size_t rsize = remote[i].len;
        nixlUcxMoPrivateMetadata *lmd;
        lmd = (nixlUcxMoPrivateMetadata *)local[i].metadataP;
        nixlUcxMoPublicMetadata *rmd;
        rmd = (nixlUcxMoPublicMetadata *)remote[i].metadataP;
        size_t lidx = lmd->eidx;
        size_t ridx = rmd->eidx;

        assert( (lidx < l_eng_cnt) && (ridx < r_eng_cnt));
    
        if (lsize != rsize) {
            // TODO: err output
            return NIXL_ERR_INVALID_PARAM;
        }

        /* Allocate internal dlists if needed */
        if (NULL == dlmatrix[lidx][ridx].first) {
            dlmatrix[lidx][ridx].first = new nixl_meta_dlist_t (
                                                local.getType(),
                                                local.isUnifiedAddr(),
                                                local.isSorted());

            dlmatrix[lidx][ridx].second = new nixl_meta_dlist_t (
                                                remote.getType(),
                                                remote.isUnifiedAddr(),
                                                remote.isSorted());
        }

        nixlMetaDesc ldesc = local[i];
        ldesc.metadataP = lmd->md;
        dlmatrix[lidx][ridx].first->addDesc(ldesc);

        nixlMetaDesc rdesc = remote[i];
        rdesc.metadataP = rmd->int_mds[lidx];
        dlmatrix[lidx][ridx].second->addDesc(rdesc);
    }

    for(size_t lidx = 0; lidx < l_eng_cnt; lidx++) {
        for(size_t ridx = 0; ridx < r_eng_cnt; ridx++) {
            string no_notif_msg;
            nixlBackendReqH *int_req;
            nixl_status_t ret;

            if (NULL == dlmatrix[lidx][ridx].first) {
                // Skip unused matrix elements
                continue;
            }
            ret = engines[lidx]->postXfer(op_int,
                                          *dlmatrix[lidx][ridx].first,
                                          *dlmatrix[lidx][ridx].second,
                                          getEngName(remote_agent, ridx),
                                          int_req);
            ret = retHelper(ret, engines[lidx], req, int_req);
            if (NIXL_SUCCESS != ret) {
                return ret;
            }
        }
    }

    switch(op) {
    case NIXL_RD_NOTIF:
    case NIXL_WR_NOTIF:
        if (opt_args==nullptr) {
            return NIXL_ERR_INVALID_PARAM;
        }
        req->notifNeed = true;
        req->notifMsg = opt_args->notifMsg;
        req->remoteAgent = remote_agent;
        break;
    default:
        break;
    }
    if (req->reqs.size()) {
        out_handle = req;
        return NIXL_IN_PROG;
    } else {
        delete req;
        return  NIXL_SUCCESS;
    }
}

nixl_status_t
nixlUcxMoEngine::checkXfer (nixlBackendReqH *handle)
{
    nixlUcxMoRequestH *req = (nixlUcxMoRequestH *)handle;
    nixlUcxMoRequestH::req_list_t &l = req->reqs;
    nixlUcxMoRequestH::req_list_it_t it;
    nixl_status_t out_ret = NIXL_SUCCESS;

    for (it = l.begin(); it != l.end(); ) {
        nixl_status_t ret;

        ret = it->first->checkXfer(it->second);
        switch (ret) {
        case NIXL_SUCCESS:
            /* Mark as completed */
            it->first->releaseReqH(it->second);
            it = l.erase(it);
            break;
        case NIXL_IN_PROG:
            out_ret = NIXL_IN_PROG;
            it++;
            break;
        default:
            /* Any other ret value is unexpected */
            return ret;
        }
    }

    if ((NIXL_SUCCESS == out_ret) && req->notifNeed) {
        nixl_status_t ret;
        ret = engines[0]->genNotif(getEngName(req->remoteAgent, 0), req->notifMsg);
        if (NIXL_SUCCESS != ret) {
            /* Mark as completed */
            return ret;
        }
    }

    return out_ret;
}

nixl_status_t
nixlUcxMoEngine::releaseReqH(nixlBackendReqH* handle)
{
    cancelRequests((nixlUcxMoRequestH *)handle);
    return NIXL_SUCCESS;
}

int
nixlUcxMoEngine::progress()
{
    int ret = 0;
    // Iterate over all elements cancelling each one
    for ( auto &e : engines ) {
        ret += e->progress();
    }
    return ret;
}

nixl_status_t
nixlUcxMoEngine::getNotifs(notif_list_t &notif_list)
{
    return engines[0]->getNotifs(notif_list);
}

nixl_status_t
nixlUcxMoEngine::genNotif(const string &remote_agent, const string &msg)
{
    return engines[0]->genNotif(getEngName(remote_agent, 0), msg);
}