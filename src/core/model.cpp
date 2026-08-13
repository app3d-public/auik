#include <auik/model.hpp>
#include <cstring>

namespace auik
{
    static void clear_model_pipeline_cache(ModelDB *db);

    AUIK_EXPORT ModelField *find_model_field(ModelRecord &record, ModelFieldID field_id)
    {
        for (auto *field : record.fields)
            if (field && field->id == field_id) return field;
        return nullptr;
    }

    AUIK_EXPORT const ModelField *find_model_field(const ModelRecord &record, ModelFieldID field_id)
    {
        for (const auto *field : record.fields)
            if (field && field->id == field_id) return field;
        return nullptr;
    }

    AUIK_EXPORT bool Model::write_field(ModelRecordID record_id, ModelFieldID field_id, const void *src)
    {
        auto *record = find_record(record_id);
        auto *field = record ? find_model_field(*record, field_id) : nullptr;
        void *dst = field ? field->data() : nullptr;
        const u32 byte_size = field ? field->size() : 0u;
        if (!dst || !src || byte_size == 0u) return false;
        std::memcpy(dst, src, byte_size);
        return true;
    }

    AUIK_EXPORT ModelRecordID make_generated_model_record_id(ModelRecord &record)
    {
        if (record.id != AUIK_MODEL_RECORD_ID_INVALID) return record.id;
        static acul::id_gen gen{};
        ModelRecordID id = AUIK_MODEL_RECORD_ID_INVALID;
        while (id == AUIK_MODEL_RECORD_ID_INVALID) id = gen();
        return id;
    }

    AUIK_EXPORT void release_model_record_fields(ModelRecord &record)
    {
        for (auto *field : record)
            if (field) field->release();
        record.fields.clear();
    }

    static void rebuild_model_record_indices(Model &model)
    {
        model.record_indices.clear();
        for (u32 index = 0u; index < model.records.size(); ++index)
        {
            const ModelRecordID record_id = model.records[index].id;
            if (record_id == AUIK_MODEL_RECORD_ID_INVALID) continue;
            model.record_indices.emplace(record_id, index);
        }
    }

    AUIK_EXPORT bool register_model(ModelDB *db, ModelID id, Model model, PFN_destroy_model destroy)
    {
        if (!db || id == 0u || find_model(db, id)) return false;
        rebuild_model_record_indices(model);
        db->models.emplace(id, ModelDB::ModelEntry{id, std::move(model), destroy});
        db->synced = false;
        return true;
    }

    AUIK_EXPORT bool unregister_model(ModelDB *db, ModelID id)
    {
        if (!db) return false;
        const auto it = db->models.find(id);
        if (it == db->models.end()) return false;
        if (it->second.destroy) it->second.destroy(&it->second.model);
        db->models.erase(it);
        db->synced = false;
        return true;
    }

    AUIK_EXPORT bool register_model_pipeline(ModelDB *db, u64 id, ModelPipelineNode *pipeline,
                                             PFN_destroy_model_pipeline destroy)
    {
        if (!db || id == 0u || !pipeline || find_model_pipeline(db, id)) return false;
        pipeline->id = id;
        db->pipelines.emplace(id, ModelDB::PipelineEntry{id, pipeline, destroy});
        db->synced = false;
        return true;
    }

    AUIK_EXPORT void destroy_model_fields(Model *model)
    {
        if (!model) return;
        for (auto &record : *model)
            for (auto *field : record)
                if (field) field->release();
    }

    AUIK_EXPORT ModelID make_generated_model_id()
    {
        static acul::id_gen gen{};
        ModelID id = 0u;
        while (id == 0u) id = gen();
        return id;
    }

    AUIK_EXPORT u32 get_model_pipeline_intermediate_stride(ModelPipelineNode *pipeline)
    {
        u32 stride = 0u;
        for (ModelPipelineNode *node = pipeline; node; node = node->next)
        {
            if (node->next) stride = stride > node->dst_size ? stride : node->dst_size;
            if (node != pipeline) stride = stride > node->src_size ? stride : node->src_size;
        }
        return stride;
    }

    AUIK_EXPORT void sync_model_db(ModelDB *db)
    {
        if (!db) return;

        u32 max_count = 1u;
        for (auto &entry : db->models)
        {
            rebuild_model_record_indices(entry.second.model);
            max_count = max_count > entry.second.model.record_count() ? max_count : entry.second.model.record_count();
        }

        u32 max_stride = 0u;
        for (const auto &entry : db->pipelines)
            max_stride = max_stride > get_model_pipeline_intermediate_stride(entry.second.pipeline)
                             ? max_stride
                             : get_model_pipeline_intermediate_stride(entry.second.pipeline);

        db->_pipeline_allocation_input_stride = max_stride;
        db->_pipeline_allocation_count = max_count;
        const u32 required = max_stride * max_count * 2u;
        if (db->_pipeline_allocation_pool.size() < required) db->_pipeline_allocation_pool.resize(required);
        db->synced = true;
    }

    AUIK_EXPORT void ModelDB::clear()
    {
        for (auto *binding : _bindings)
        {
            if (!binding) continue;
            detach_model_binding(*binding);
            acul::release(binding);
        }
        _bindings.clear();
        for (auto &entry : pipelines)
            if (entry.second.destroy) entry.second.destroy(entry.second.pipeline);
        pipelines.clear();

        for (auto &entry : models)
            if (entry.second.destroy) entry.second.destroy(&entry.second.model);
        models.clear();

        _pipeline_allocation_pool.clear();
        clear_model_pipeline_cache(this);
        _pipeline_allocation_input_stride = 0u;
        _pipeline_allocation_count = 0u;
        _pipeline_cache_scope_depth = 0u;
        synced = false;
    }

    AUIK_EXPORT void clear_model_db(ModelDB *db)
    {
        if (db) db->clear();
    }

    AUIK_EXPORT bool model_pipeline_cache_active(const ModelDB *db)
    {
        return db && db->_pipeline_cache_scope_depth != 0u;
    }

    static void clear_model_pipeline_cache(ModelDB *db)
    {
        if (!db) return;
        for (auto &entry : db->_pipeline_cache_entries)
            if (entry.destroy) entry.destroy(entry.data);
        db->_pipeline_cache_entries.clear();
    }

    AUIK_EXPORT void begin_model_pipeline_cache(ModelDB *db)
    {
        if (!db) return;
        if (db->_pipeline_cache_scope_depth++ == 0u) clear_model_pipeline_cache(db);
    }

    AUIK_EXPORT void end_model_pipeline_cache(ModelDB *db)
    {
        if (!db || db->_pipeline_cache_scope_depth == 0u) return;
        if (--db->_pipeline_cache_scope_depth == 0u) clear_model_pipeline_cache(db);
    }

    AUIK_EXPORT const ModelPipelineCacheEntry *find_model_pipeline_cache_entry(const ModelDB *db,
                                                                               const ModelPipelineCacheKey &key)
    {
        if (!model_pipeline_cache_active(db)) return nullptr;
        for (const auto &entry : db->_pipeline_cache_entries)
            if (entry.key == key) return &entry;
        return nullptr;
    }

    AUIK_EXPORT bool is_model_binding_valid(const ModelBinding &binding)
    {
        return binding.db && binding.db->synced && find_model(binding.db, binding.model_id) != nullptr;
    }

    AUIK_EXPORT bool is_model_field_access_valid(ModelDB *db, ModelID model_id, const ModelFieldAccess &access)
    {
        return db && db->synced && find_model(db, model_id) && access.record_id != AUIK_MODEL_RECORD_ID_INVALID &&
               access.field_id != AUIK_MODEL_FIELD_ID_INVALID;
    }

    static void erase_model_field_model_binding(acul::vector<ModelFieldModelBinding> &items, ModelBinding *binding,
                                                ModelRecordID record_id)
    {
        for (size_t i = 0; i < items.size();)
        {
            if (items[i].binding == binding && items[i].record_id == record_id) items.erase(items.begin() + i);
            else ++i;
        }
    }

    static void attach_model_binding_record_fields(ModelBinding &binding, ModelRecordID record_id)
    {
        if (!binding.db || !binding.db->synced || record_id == AUIK_MODEL_RECORD_ID_INVALID) return;
        auto *model = find_model(binding.db, binding.model_id);
        auto *record = model ? model->find_record(record_id) : nullptr;
        if (!record) return;
        for (auto *field : *record)
            if (field) field->model_bindings.push_back(ModelFieldModelBinding{&binding, record_id});
    }

    static void attach_model_binding_default_access(ModelBinding &binding)
    {
        if (!binding.db || !binding.db->synced) return;
        ModelFieldAccess access{};
        if (!access_model_binding_field(binding, access)) return;
        auto *field = resolve_model_field(binding.db, binding.model_id, access);
        if (field) field->model_bindings.push_back(ModelFieldModelBinding{&binding, access.record_id});
    }

    static void detach_model_binding_default_access(ModelBinding &binding)
    {
        if (!binding.db || !binding.db->synced) return;
        ModelFieldAccess access{};
        if (!access_model_binding_field(binding, access)) return;
        auto *field = resolve_model_field(binding.db, binding.model_id, access);
        if (field) erase_model_field_model_binding(field->model_bindings, &binding, access.record_id);
    }

    static void detach_model_binding_record_fields(ModelBinding &binding, ModelRecordID record_id)
    {
        if (!binding.db || !binding.db->synced || record_id == AUIK_MODEL_RECORD_ID_INVALID) return;
        auto *model = find_model(binding.db, binding.model_id);
        auto *record = model ? model->find_record(record_id) : nullptr;
        if (!record) return;
        for (auto *field : *record)
            if (field) erase_model_field_model_binding(field->model_bindings, &binding, record_id);
    }

    static void detach_model_binding_fields(ModelBinding &binding)
    {
        if (!binding.db || !binding.db->synced) return;
        detach_model_binding_default_access(binding);
        const auto records = binding.records;
        for (ModelRecordID record_id : records) detach_model_binding_record_fields(binding, record_id);
    }

    AUIK_EXPORT void *get_model_field_access_data(ModelDB *db, ModelID model_id, const ModelFieldAccess &access)
    {
        assert(db && db->synced);
        auto *model = is_model_field_access_valid(db, model_id, access) ? find_model(db, model_id) : nullptr;
        return model ? model->field_data(access.record_id, access.field_id) : nullptr;
    }

    AUIK_EXPORT const void *get_model_field_access_data(const ModelDB *db, ModelID model_id,
                                                        const ModelFieldAccess &access)
    {
        assert(db && db->synced);
        const auto *model = is_model_field_access_valid(const_cast<ModelDB *>(db), model_id, access)
                                ? find_const_model(db, model_id)
                                : nullptr;
        return model ? model->field_data(access.record_id, access.field_id) : nullptr;
    }

    AUIK_EXPORT bool write_model_field_access(ModelDB *db, ModelID model_id, const ModelFieldAccess &access,
                                              const void *src)
    {
        assert(db && db->synced);
        auto *model = is_model_field_access_valid(db, model_id, access) ? find_model(db, model_id) : nullptr;
        return model && model->write_field(access.record_id, access.field_id, src);
    }

    AUIK_EXPORT ModelField *resolve_model_field(ModelDB *db, ModelID model_id, const ModelFieldAccess &access)
    {
        assert(db && db->synced);
        auto *model = is_model_field_access_valid(db, model_id, access) ? find_model(db, model_id) : nullptr;
        auto *record = model ? model->find_record(access.record_id) : nullptr;
        return record ? find_model_field(*record, access.field_id) : nullptr;
    }

    AUIK_EXPORT const ModelField *resolve_model_field(const ModelDB *db, ModelID model_id,
                                                      const ModelFieldAccess &access)
    {
        assert(db && db->synced);
        const auto *model = is_model_field_access_valid(const_cast<ModelDB *>(db), model_id, access)
                                ? find_const_model(db, model_id)
                                : nullptr;
        const auto *record = model ? model->find_record(access.record_id) : nullptr;
        return record ? find_model_field(*record, access.field_id) : nullptr;
    }

    AUIK_EXPORT ModelRecordID first_model_record_id(ModelDB *db, ModelID model_id)
    {
        auto *model = find_model(db, model_id);
        return model && model->record_count() != 0u ? model->record_id_at(0) : AUIK_MODEL_RECORD_ID_INVALID;
    }

    AUIK_EXPORT bool default_access_model_binding_field(ModelBinding *binding, ModelRecordID record_id,
                                                        ModelFieldID field_id, ModelFieldAccess *out)
    {
        if (!binding || !out || !is_model_binding_valid(*binding)) return false;

        auto *access =
            binding->access_data ? static_cast<ModelFieldAccess *>(binding->access_data) : &binding->default_access;
        if (record_id == AUIK_MODEL_RECORD_ID_INVALID) record_id = access->record_id;
        if (field_id == AUIK_MODEL_FIELD_ID_INVALID) field_id = access->field_id;
        if (record_id == AUIK_MODEL_RECORD_ID_INVALID || field_id == AUIK_MODEL_FIELD_ID_INVALID) return false;
        *out = ModelFieldAccess{record_id, field_id, access->pipeline};
        return resolve_model_field(binding->db, binding->model_id, *out) != nullptr;
    }

    AUIK_EXPORT bool access_model_binding_field(ModelBinding &binding, ModelRecordID record_id, ModelFieldID field_id,
                                                ModelFieldAccess &out)
    {
        assert(binding.db && binding.db->synced);
        if (binding.access_field) return binding.access_field(&binding, record_id, field_id, &out);
        return default_access_model_binding_field(&binding, record_id, field_id, &out);
    }

    AUIK_EXPORT bool access_model_binding_field(ModelBinding &binding, ModelFieldAccess &out)
    {
        return access_model_binding_field(binding, AUIK_MODEL_RECORD_ID_INVALID, AUIK_MODEL_FIELD_ID_INVALID, out);
    }

    AUIK_EXPORT void dispatch_model_field(ModelField &field)
    {
        for (auto it = field.model_bindings_begin(); it != field.model_bindings_end(); ++it)
            if (it->binding) begin_model_pipeline_cache(it->binding->db);

        for (auto it = field.model_bindings_begin(); it != field.model_bindings_end(); ++it)
            if (it->binding) dispatch_model_binding_field(*it->binding, it->record_id, field.id);

        for (auto it = field.model_bindings_begin(); it != field.model_bindings_end(); ++it)
            if (it->binding) end_model_pipeline_cache(it->binding->db);
    }

    AUIK_EXPORT bool process_model_pipeline(ModelDB *db, ModelPipelineNode *pipeline, const void *src, void *dst)
    {
        assert(db && db->synced);
        if (!db || !db->synced || !pipeline || !src || !dst) return false;

        const void *current_src = src;
        u32 scratch_index = 0u;
        for (ModelPipelineNode *node = pipeline; node; node = node->next)
        {
            if (!node->process) return false;
            const bool last = node->next == nullptr;
            void *current_dst = dst;
            if (!last)
            {
                if (!node->dst_size || node->dst_size > db->_pipeline_allocation_input_stride) return false;
                const u32 offset = db->_pipeline_allocation_input_stride * scratch_index;
                current_dst = db->_pipeline_allocation_pool.data() + offset;
            }
            if (!node->process(node->data, current_src, current_dst)) return false;
            current_src = current_dst;
            scratch_index = 1u - scratch_index;
        }
        return true;
    }

    AUIK_EXPORT bool process_model_pipeline_reverse(ModelDB *db, ModelPipelineNode *pipeline, const void *src,
                                                    void *dst)
    {
        assert(db && db->synced);
        if (!db || !db->synced || !pipeline || !src || !dst) return false;

        acul::vector<ModelPipelineNode *> nodes;
        for (ModelPipelineNode *node = pipeline; node; node = node->next) nodes.push_back(node);
        if (nodes.empty()) return false;

        const void *current_src = src;
        u32 scratch_index = 0u;
        for (u32 i = static_cast<u32>(nodes.size()); i-- > 0u;)
        {
            ModelPipelineNode *node = nodes[i];
            if (!node->process_reverse) return false;
            const bool last = i == 0u;
            void *current_dst = dst;
            if (!last)
            {
                if (!node->src_size || node->src_size > db->_pipeline_allocation_input_stride) return false;
                const u32 offset = db->_pipeline_allocation_input_stride * scratch_index;
                current_dst = db->_pipeline_allocation_pool.data() + offset;
            }
            if (!node->process_reverse(node->data, current_src, current_dst)) return false;
            current_src = current_dst;
            scratch_index = 1u - scratch_index;
        }
        return true;
    }

    AUIK_EXPORT bool process_model_field_access(ModelDB *db, ModelID model_id, const ModelFieldAccess &access,
                                                void *dst)
    {
        auto *field = resolve_model_field(db, model_id, access);
        const void *src = field ? field->data() : nullptr;
        if (!src || !dst) return false;

        return process_model_pipeline(db, access.pipeline, src, dst);
    }

    AUIK_EXPORT bool process_model_field_access_reverse(ModelDB *db, ModelID model_id, const ModelFieldAccess &access,
                                                        const void *src)
    {
        void *dst = get_model_field_access_data(db, model_id, access);
        return dst && process_model_pipeline_reverse(db, access.pipeline, src, dst);
    }

    AUIK_EXPORT ModelRecordID *find_model_binding_record(ModelBinding &binding, ModelRecordID record_id)
    {
        assert(binding.db && binding.db->synced);
        for (auto &record : binding.records)
            if (record == record_id) return &record;
        return nullptr;
    }

    AUIK_EXPORT void attach_model_binding(ModelBinding &binding)
    {
        assert(binding.db && binding.db->synced);
        auto *model = find_model(binding.db, binding.model_id);
        if (!model) return;
        erase_model_binding_ptr(model->bindings, &binding);
        model->bindings.push_back(&binding);
        attach_model_binding_default_access(binding);
        for (ModelRecordID record_id : binding.records) attach_model_binding_record_fields(binding, record_id);
    }

    AUIK_EXPORT void detach_model_binding(ModelBinding &binding)
    {
        if (!binding.db || !binding.db->synced) return;
        detach_model_binding_fields(binding);
        if (auto *model = find_model(binding.db, binding.model_id)) erase_model_binding_ptr(model->bindings, &binding);
    }

    AUIK_EXPORT bool ModelDB::register_binding(ModelBinding *binding)
    {
        if (!binding || binding->db || !find_model(this, binding->model_id)) return false;
        for (auto *registered : _bindings)
            if (registered == binding) return false;

        if (!synced) sync_model_db(this);
        if (binding->default_access.record_id == AUIK_MODEL_RECORD_ID_INVALID &&
            binding->default_access.field_id != AUIK_MODEL_FIELD_ID_INVALID)
            binding->default_access.record_id = first_model_record_id(this, binding->model_id);

        binding->db = this;
        _bindings.push_back(binding);
        attach_model_binding(*binding);

        if (binding->default_access.field_id != AUIK_MODEL_FIELD_ID_INVALID)
            dispatch_model_binding_field(*binding, binding->default_access.record_id, binding->default_access.field_id);
        return true;
    }

    AUIK_EXPORT bool ModelDB::unregister_binding(ModelBinding *binding)
    {
        if (!binding || binding->db != this) return false;
        for (size_t index = 0; index < _bindings.size(); ++index)
        {
            if (_bindings[index] != binding) continue;
            detach_model_binding(*binding);
            binding->db = nullptr;
            _bindings.erase(_bindings.begin() + index);
            acul::release(binding);
            return true;
        }
        return false;
    }

    AUIK_EXPORT void rebuild_model_binding_records(ModelBinding &binding)
    {
        assert(binding.db && binding.db->synced);
        detach_model_binding_fields(binding);
        binding.records.clear();
        auto *model = find_model(binding.db, binding.model_id);
        if (!model) return;
        const u32 count = model->record_count();
        binding.records.reserve(count);
        for (u32 index = 0; index < count; ++index)
        {
            const ModelRecordID record_id = model->record_id_at(index);
            if (record_id == AUIK_MODEL_RECORD_ID_INVALID) continue;
            binding.records.push_back(record_id);
            attach_model_binding_record_fields(binding, record_id);
        }
    }

    AUIK_EXPORT void default_dispatch_model_binding(ModelBinding *binding, const ModelRecordsEvent &event)
    {
        assert(binding && binding->db && binding->db->synced);
        switch (event.op)
        {
            case ModelRecordsOp::create:
                if (event.record_id != AUIK_MODEL_RECORD_ID_INVALID &&
                    !find_model_binding_record(*binding, event.record_id))
                {
                    const u32 insert_index = event.to_index <= binding->records.size()
                                                 ? event.to_index
                                                 : static_cast<u32>(binding->records.size());
                    binding->records.insert(binding->records.begin() + insert_index, event.record_id);
                    attach_model_binding_record_fields(*binding, event.record_id);
                }
                break;
            case ModelRecordsOp::destroy:
                detach_model_binding_record_fields(*binding, event.record_id);
                if (auto *record = find_model_binding_record(*binding, event.record_id)) binding->records.erase(record);
                break;
            case ModelRecordsOp::move:
            case ModelRecordsOp::order:
            case ModelRecordsOp::reset:
                rebuild_model_binding_records(*binding);
                break;
        }
        if (binding->on_records) binding->on_records(event);
    }

    AUIK_EXPORT void default_dispatch_model_binding_field(ModelBinding *binding, ModelRecordID record_id,
                                                          ModelFieldID field_id)
    {
        assert(binding && binding->db && binding->db->synced);
        if (binding->on_field_change) binding->on_field_change(record_id, field_id);
    }

    AUIK_EXPORT ModelRecord *append_model_record(ModelDB *db, ModelID model_id, ModelRecord record)
    {
        auto *model = find_model(db, model_id);
        if (!db || !model) return nullptr;
        const u32 index = model->record_count();
        auto &stored = model->add_record(std::move(record));
        db->synced = false;
        sync_model_db(db);
        model->dispatch_records(ModelRecordsEvent{ModelRecordsOp::create, stored.id, index, index, 1u});
        return &stored;
    }

    AUIK_EXPORT bool erase_model_record(ModelDB *db, ModelID model_id, ModelRecordID record_id, bool release_fields)
    {
        auto *model = find_model(db, model_id);
        if (!db || !model) return false;
        const u32 index = model->record_index(record_id);
        if (index == 0xFFFFFFFFu) return false;
        for (auto *binding : model->bindings)
            if (binding) detach_model_binding_record_fields(*binding, record_id);
        if (release_fields) release_model_record_fields(model->records[index]);
        model->records.erase(model->records.begin() + index);
        rebuild_model_record_indices(*model);
        db->synced = false;
        sync_model_db(db);
        model->dispatch_records(ModelRecordsEvent{ModelRecordsOp::destroy, record_id, index, index, 1u});
        return true;
    }

    AUIK_EXPORT bool move_model_record(ModelDB *db, ModelID model_id, ModelRecordID record_id, u32 to_index)
    {
        auto *model = find_model(db, model_id);
        if (!db || !model) return false;
        const u32 from_index = model->record_index(record_id);
        if (from_index == 0xFFFFFFFFu || from_index == to_index || to_index >= model->records.size()) return false;
        ModelRecord record = std::move(model->records[from_index]);
        model->records.erase(model->records.begin() + from_index);
        model->records.insert(model->records.begin() + to_index, std::move(record));
        rebuild_model_record_indices(*model);
        model->dispatch_records(ModelRecordsEvent{ModelRecordsOp::move, record_id, from_index, to_index, 1u});
        return true;
    }

    AUIK_EXPORT void reset_model_records(ModelDB *db, ModelID model_id)
    {
        auto *model = find_model(db, model_id);
        if (!db || !model) return;
        db->synced = false;
        sync_model_db(db);
        model->dispatch_records(ModelRecordsEvent{ModelRecordsOp::reset});
    }

    AUIK_EXPORT bool process_model_pipeline_batch(ModelDB *db, ModelPipelineNode *pipeline, const void *src,
                                                  u32 src_stride, void *dst, u32 dst_stride, u32 count)
    {
        assert(db && db->synced);
        if (!db || !db->synced || !pipeline || !src || src_stride == 0u || !dst || dst_stride == 0u || count == 0u)
            return false;
        if (count > db->_pipeline_allocation_count) return false;

        const void *current_src = src;
        u32 current_src_stride = src_stride;
        u32 scratch_index = 0u;
        for (ModelPipelineNode *node = pipeline; node; node = node->next)
        {
            const bool last = node->next == nullptr;
            void *current_dst = dst;
            u32 current_dst_stride = dst_stride;
            if (!last)
            {
                if (!node->dst_size || node->dst_size > db->_pipeline_allocation_input_stride) return false;
                current_dst_stride = node->dst_size;
                const u32 offset = db->_pipeline_allocation_input_stride * count * scratch_index;
                current_dst = db->_pipeline_allocation_pool.data() + offset;
            }

            if (!node->process) return false;
            for (u32 i = 0; i < count; ++i)
            {
                const auto *item_src = static_cast<const u8 *>(current_src) + current_src_stride * i;
                auto *item_dst = static_cast<u8 *>(current_dst) + current_dst_stride * i;
                if (!node->process(node->data, item_src, item_dst)) return false;
            }

            current_src = current_dst;
            current_src_stride = current_dst_stride;
            scratch_index = 1u - scratch_index;
        }
        return true;
    }

    AUIK_EXPORT bool process_model_binding_field_batch(ModelBinding &binding, ModelFieldID field_id,
                                                       ModelPipelineNode *pipeline, void *dst, u32 dst_stride)
    {
        assert(binding.db && binding.db->synced);
        auto *model = find_model(binding.db, binding.model_id);
        if (!binding.db || !binding.db->synced || !model || !pipeline || !dst || dst_stride == 0u ||
            binding.records.empty())
            return false;

        if (binding.access_field == default_access_model_binding_field)
        {
            ModelBatchData batch =
                model->field_data_batch(binding.records.data(), static_cast<u32>(binding.records.size()), field_id);
            if (batch.contiguous())
                return process_model_pipeline_batch(binding.db, pipeline, batch.data, batch.stride, dst, dst_stride,
                                                    batch.count);
        }

        for (u32 i = 0; i < binding.records.size(); ++i)
        {
            ModelFieldAccess access{};
            if (!access_model_binding_field(binding, binding.records[i], field_id, access)) return false;
            const void *src = get_model_field_access_data(binding.db, binding.model_id, access);
            if (!src) return false;
            auto *item_dst = static_cast<u8 *>(dst) + dst_stride * i;
            if (access.pipeline)
            {
                if (!process_model_field_access(binding.db, binding.model_id, access, item_dst)) return false;
                if (!process_model_pipeline(binding.db, pipeline, item_dst, item_dst)) return false;
            }
            else if (!process_model_pipeline(binding.db, pipeline, src, item_dst)) return false;
        }
        return true;
    }

} // namespace auik
