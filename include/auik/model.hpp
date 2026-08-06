#pragma once

#include <acul/functional/unique_function.hpp>
#include <acul/hash/hashmap.hpp>
#include <acul/hash/utils.hpp>
#include <acul/memory/alloc.hpp>
#include <acul/scalars.hpp>
#include <acul/string/string.hpp>
#include <acul/vector.hpp>
#include <amal/geometric.hpp>
#include <auik/symbol_export.h>

#define AUIK_MODEL_RECORD_ID_INVALID 0u
#define AUIK_MODEL_FIELD_ID_INVALID  0u

namespace auik
{
    class Widget;
    struct Model;
    struct ModelBinding;
    struct ModelField;

    using ModelID = u64;
    using ModelRecordID = u64;
    using ModelFieldID = u32;

    enum class ModelRecordsOp : u8
    {
        create,
        destroy,
        move,
        order,
        reset
    };

    struct ModelRecordsEvent
    {
        ModelRecordsOp op = ModelRecordsOp::reset;
        ModelRecordID record_id = AUIK_MODEL_RECORD_ID_INVALID;
        u32 from_index = 0u;
        u32 to_index = 0u;
        u32 count = 1u;
    };

    struct ModelBatchData
    {
        const void *data = nullptr;
        const void *const *items = nullptr;
        u32 stride = 0u;
        u32 count = 0u;

        bool contiguous() const { return data != nullptr && stride != 0u; }
        bool indexed() const { return items != nullptr; }
        explicit operator bool() const { return count != 0u && (contiguous() || indexed()); }
    };

    struct ModelBinding;
    struct ModelFieldModelBinding
    {
        ModelBinding *binding = nullptr;
        ModelRecordID record_id = AUIK_MODEL_RECORD_ID_INVALID;
    };
    AUIK_EXPORT void dispatch_model_field(ModelField &field);

    struct ModelField
    {
        using model_binding_container = acul::vector<ModelFieldModelBinding>;
        using value_type = ModelFieldModelBinding;
        using iterator = model_binding_container::iterator;
        using const_iterator = model_binding_container::const_iterator;
        using model_binding_iterator = model_binding_container::iterator;
        using const_model_binding_iterator = model_binding_container::const_iterator;

        ModelFieldID id = AUIK_MODEL_FIELD_ID_INVALID;
        model_binding_container model_bindings;

        virtual ~ModelField() = default;
        virtual void *data() { return nullptr; }
        virtual const void *data() const { return nullptr; }
        virtual u32 size() const { return 0u; }
        virtual void release() { acul::release(this); }

        template <class T>
        T *as()
        { return static_cast<T *>(data()); }

        template <class T>
        const T *as() const
        { return static_cast<const T *>(data()); }

        iterator begin() { return model_bindings.begin(); }
        iterator end() { return model_bindings.end(); }
        const_iterator begin() const { return model_bindings.begin(); }
        const_iterator end() const { return model_bindings.end(); }
        const_iterator cbegin() const { return model_bindings.cbegin(); }
        const_iterator cend() const { return model_bindings.cend(); }
        model_binding_iterator model_bindings_begin() { return model_bindings.begin(); }
        model_binding_iterator model_bindings_end() { return model_bindings.end(); }
        const_model_binding_iterator model_bindings_begin() const { return model_bindings.begin(); }
        const_model_binding_iterator model_bindings_end() const { return model_bindings.end(); }
        const_model_binding_iterator model_bindings_cbegin() const { return model_bindings.cbegin(); }
        const_model_binding_iterator model_bindings_cend() const { return model_bindings.cend(); }
    };

    struct ModelRefField final : ModelField
    {
        void *ptr = nullptr;
        u32 byte_size = 0u;

        ModelRefField() = default;
        ModelRefField(ModelFieldID field_id, void *field_data, u32 field_size) : ptr(field_data), byte_size(field_size)
        { id = field_id; }

        void *data() override { return ptr; }
        const void *data() const override { return ptr; }
        u32 size() const override { return byte_size; }
        void release() override { acul::release(this); }
    };

    template <class T>
    struct ModelValueField final : ModelField
    {
        T value{};

        ModelValueField() = default;
        explicit ModelValueField(ModelFieldID field_id) { id = field_id; }
        ModelValueField(ModelFieldID field_id, T field_value) : value(std::move(field_value)) { id = field_id; }

        template <class U>
        void set_value(U &&next)
        {
            value = T(std::forward<U>(next));
            dispatch_model_field(*this);
        }

        void *data() override { return &value; }
        const void *data() const override { return &value; }
        u32 size() const override { return sizeof(T); }
        void release() override { acul::release(this); }
    };

    template <class T, class... Args>
    inline ModelValueField<T> *make_model_field(ModelFieldID field_id, Args &&...args)
    { return acul::alloc<ModelValueField<T>>(field_id, T(std::forward<Args>(args)...)); }

    struct ModelRecord
    {
        using field_container = acul::vector<ModelField *>;
        using value_type = ModelField *;
        using iterator = field_container::iterator;
        using const_iterator = field_container::const_iterator;

        ModelRecordID id = AUIK_MODEL_RECORD_ID_INVALID;
        void *data = nullptr;
        field_container fields;

        template <class T>
        T *as()
        { return static_cast<T *>(data); }

        template <class T>
        const T *as() const
        { return static_cast<const T *>(data); }

        iterator begin() { return fields.begin(); }
        iterator end() { return fields.end(); }
        const_iterator begin() const { return fields.begin(); }
        const_iterator end() const { return fields.end(); }
        const_iterator cbegin() const { return fields.cbegin(); }
        const_iterator cend() const { return fields.cend(); }
    };

    using PFN_model_record_id = ModelRecordID (*)(ModelRecord &record);
    using PFN_model_field_data_batch = ModelBatchData (*)(const ModelRecordID *record_ids, u32 count,
                                                          ModelFieldID field_id, void *data);
    using PFN_present_model_record = void (*)(ModelBinding *binding, ModelRecord &record, u32 record_index,
                                              Widget **widgets, u32 widget_count, void *data);

    struct ModelRecordPresenter
    {
        void *data = nullptr;
        acul::vector<ModelFieldID> field_ids;
        PFN_present_model_record present_record = nullptr;
    };

    AUIK_EXPORT ModelRecordID make_generated_model_record_id(ModelRecord &record);

    AUIK_EXPORT ModelField *find_model_field(ModelRecord &record, ModelFieldID field_id);
    AUIK_EXPORT const ModelField *find_model_field(const ModelRecord &record, ModelFieldID field_id);

    struct Model
    {
        using record_container = acul::vector<ModelRecord>;
        using record_index_container = acul::hashmap<ModelRecordID, u32>;
        using binding_container = acul::vector<ModelBinding *>;
        using value_type = ModelRecord;
        using iterator = record_container::iterator;
        using const_iterator = record_container::const_iterator;
        using binding_iterator = binding_container::iterator;
        using const_binding_iterator = binding_container::const_iterator;

        void *data = nullptr;
        PFN_model_record_id make_record_id_cb = nullptr;
        PFN_model_field_data_batch field_data_batch_callback = nullptr;
        record_container records;
        record_index_container record_indices;
        binding_container bindings;

        template <class T>
        T *as()
        { return static_cast<T *>(data); }

        template <class T>
        const T *as() const
        { return static_cast<const T *>(data); }

        inline u32 record_count() const { return static_cast<u32>(records.size()); }

        inline ModelRecordID record_id_at(u32 index) const
        { return index < records.size() ? records[index].id : AUIK_MODEL_RECORD_ID_INVALID; }

        inline u32 record_index(ModelRecordID record_id) const
        {
            const auto it = record_indices.find(record_id);
            if (it != record_indices.end()) return it->second;
            return 0xFFFFFFFFu;
        }

        inline u32 field_count() const { return records.empty() ? 0u : static_cast<u32>(records[0].fields.size()); }

        inline ModelFieldID field_id_at(u32 index) const
        {
            if (records.empty() || index >= records[0].fields.size()) return AUIK_MODEL_FIELD_ID_INVALID;
            return records[0].fields[index] ? records[0].fields[index]->id : AUIK_MODEL_FIELD_ID_INVALID;
        }
        inline u32 field_index(ModelFieldID field_id) const
        {
            const u32 count = field_count();
            for (u32 index = 0; index < count; ++index)
                if (field_id_at(index) == field_id) return index;
            return 0xFFFFFFFFu;
        }

        inline ModelRecord *find_record(ModelRecordID record_id)
        {
            const u32 index = record_index(record_id);
            return index < records.size() ? &records[index] : nullptr;
        }

        inline const ModelRecord *find_record(ModelRecordID record_id) const
        {
            const u32 index = record_index(record_id);
            return index < records.size() ? &records[index] : nullptr;
        }

        inline void *field_data(ModelRecordID record_id, ModelFieldID field_id)
        {
            auto *record = find_record(record_id);
            auto *field = record ? find_model_field(*record, field_id) : nullptr;
            return field ? field->data() : nullptr;
        }

        inline const void *field_data(ModelRecordID record_id, ModelFieldID field_id) const
        {
            const auto *record = find_record(record_id);
            const auto *field = record ? find_model_field(*record, field_id) : nullptr;
            return field ? field->data() : nullptr;
        }

        AUIK_EXPORT bool write_field(ModelRecordID record_id, ModelFieldID field_id, const void *src);

        inline ModelBatchData field_data_batch(const ModelRecordID *record_ids, u32 count, ModelFieldID field_id) const
        {
            if (field_data_batch_callback) return field_data_batch_callback(record_ids, count, field_id, data);
            return {};
        }

        inline void dispatch_records(const ModelRecordsEvent &event);

        inline ModelRecordID make_record_id(ModelRecord &record)
        { return make_record_id_cb ? make_record_id_cb(record) : record.id; }

        inline ModelRecord &add_record(ModelRecord record)
        {
            record.id = make_record_id(record);
            records.push_back(std::move(record));
            if (records.back().id != AUIK_MODEL_RECORD_ID_INVALID)
            {
                record_indices.emplace(records.back().id, static_cast<u32>(records.size() - 1u));
            }
            return records.back();
        }

        inline ModelRecord &add_record(ModelRecordID record_id, void *data)
        {
            ModelRecord record{};
            record.id = record_id;
            record.data = data;
            return add_record(std::move(record));
        }

        iterator begin() { return records.begin(); }
        iterator end() { return records.end(); }
        const_iterator begin() const { return records.begin(); }
        const_iterator end() const { return records.end(); }
        const_iterator cbegin() const { return records.cbegin(); }
        const_iterator cend() const { return records.cend(); }

        binding_iterator bindings_begin() { return bindings.begin(); }
        binding_iterator bindings_end() { return bindings.end(); }
        const_binding_iterator bindings_begin() const { return bindings.begin(); }
        const_binding_iterator bindings_end() const { return bindings.end(); }
        const_binding_iterator bindings_cbegin() const { return bindings.cbegin(); }
        const_binding_iterator bindings_cend() const { return bindings.cend(); }
    };

    inline ModelField *add_model_field(ModelRecord &record, ModelField *field)
    {
        if (!field) return nullptr;
        record.fields.push_back(field);
        return field;
    }

    AUIK_EXPORT void release_model_record_fields(ModelRecord &record);

    template <class T = acul::string>
    inline ModelValueField<T> *get_value_model_field(Model *model,
                                                     ModelRecordID record_id = AUIK_MODEL_RECORD_ID_INVALID,
                                                     ModelFieldID field_id = 1u)
    {
        if (!model || model->records.empty()) return nullptr;
        auto *record = record_id == AUIK_MODEL_RECORD_ID_INVALID ? &model->records[0] : model->find_record(record_id);
        auto *field = record ? find_model_field(*record, field_id) : nullptr;
        return field ? static_cast<ModelValueField<T> *>(field) : nullptr;
    }

    template <class T = acul::string>
    inline const ModelValueField<T> *get_value_model_field(const Model *model,
                                                           ModelRecordID record_id = AUIK_MODEL_RECORD_ID_INVALID,
                                                           ModelFieldID field_id = 1u)
    {
        if (!model || model->records.empty()) return nullptr;
        const auto *record =
            record_id == AUIK_MODEL_RECORD_ID_INVALID ? &model->records[0] : model->find_record(record_id);
        const auto *field = record ? find_model_field(*record, field_id) : nullptr;
        return field ? static_cast<const ModelValueField<T> *>(field) : nullptr;
    }

    using PFN_model_pipeline_process = bool (*)(void *data, const void *src, void *dst);
    struct ModelPipelineNode
    {
        PFN_model_pipeline_process process = nullptr;
        PFN_model_pipeline_process process_reverse = nullptr;
        u32 src_size = 0u;
        u32 dst_size = 0u;
        void *data = nullptr;
        ModelPipelineNode *next = nullptr;
    };

    struct ModelPipelineCacheKey
    {
        ModelID model_id = 0u;
        ModelRecordID record_id = AUIK_MODEL_RECORD_ID_INVALID;
        ModelFieldID field_id = AUIK_MODEL_FIELD_ID_INVALID;
        const ModelPipelineNode *node = nullptr;
    };

    inline bool operator==(const ModelPipelineCacheKey &a, const ModelPipelineCacheKey &b)
    { return a.model_id == b.model_id && a.record_id == b.record_id && a.field_id == b.field_id && a.node == b.node; }

    struct ModelPipelineCacheEntry
    {
        ModelPipelineCacheKey key;
        const void *data = nullptr;
        u32 size = 0u;
        u32 pool_offset = 0u;
        bool owned = false;
    };

    using PFN_destroy_model = void (*)(Model *);
    using PFN_destroy_model_pipeline = void (*)(ModelPipelineNode *);

    struct ModelDB
    {
        struct ModelEntry
        {
            ModelID id = 0u;
            Model model;
            PFN_destroy_model destroy = nullptr;
        };

        struct PipelineEntry
        {
            u64 id = 0u;
            ModelPipelineNode *pipeline = nullptr;
            PFN_destroy_model_pipeline destroy = nullptr;
        };

        using model_container = acul::hashmap<ModelID, ModelEntry>;
        using pipeline_container = acul::hashmap<u64, PipelineEntry>;
        using binding_container = acul::vector<ModelBinding *>;
        using model_iterator = model_container::iterator;
        using const_model_iterator = model_container::const_iterator;
        using pipeline_iterator = pipeline_container::iterator;
        using const_pipeline_iterator = pipeline_container::const_iterator;

        model_container models;
        pipeline_container pipelines;
        acul::vector<u8> _pipeline_allocation_pool;
        acul::vector<u8> _pipeline_cache_pool;
        acul::vector<ModelPipelineCacheEntry> _pipeline_cache_entries;
        u32 _pipeline_allocation_input_stride = 0u;
        u32 _pipeline_allocation_count = 0u;
        u32 _pipeline_cache_pool_used = 0u;
        u32 _pipeline_cache_scope_depth = 0u;
        bool synced = false;

        ~ModelDB() { clear(); }

        AUIK_EXPORT void clear();
        // Takes ownership of binding and activates it for this database.
        AUIK_EXPORT bool register_binding(ModelBinding *binding);
        AUIK_EXPORT bool unregister_binding(ModelBinding *binding);

    private:
        binding_container _bindings;

    public:

        model_iterator models_begin() { return models.begin(); }
        model_iterator models_end() { return models.end(); }
        const_model_iterator models_begin() const { return models.begin(); }
        const_model_iterator models_end() const { return models.end(); }
        const_model_iterator models_cbegin() const { return models.cbegin(); }
        const_model_iterator models_cend() const { return models.cend(); }

        pipeline_iterator pipelines_begin() { return pipelines.begin(); }
        pipeline_iterator pipelines_end() { return pipelines.end(); }
        const_pipeline_iterator pipelines_begin() const { return pipelines.begin(); }
        const_pipeline_iterator pipelines_end() const { return pipelines.end(); }
        const_pipeline_iterator pipelines_cbegin() const { return pipelines.cbegin(); }
        const_pipeline_iterator pipelines_cend() const { return pipelines.cend(); }
    };

    struct ModelFieldAccess
    {
        ModelRecordID record_id = AUIK_MODEL_RECORD_ID_INVALID;
        ModelFieldID field_id = AUIK_MODEL_FIELD_ID_INVALID;
        ModelPipelineNode *pipeline = nullptr;
    };

    using PFN_model_binding_dispatch = void (*)(ModelBinding *, const ModelRecordsEvent &);
    using PFN_model_binding_field_dispatch = void (*)(ModelBinding *, ModelRecordID, ModelFieldID);
    using PFN_model_binding_access_field = bool (*)(ModelBinding *, ModelRecordID, ModelFieldID, ModelFieldAccess *);

    struct ModelBinding
    {
        ModelDB *db = nullptr;
        ModelID model_id = 0u;
        acul::vector<ModelRecordID> records;
        void *access_data = nullptr;
        ModelFieldAccess default_access;
        ModelRecordPresenter presenter;
        acul::unique_function<void(const ModelRecordsEvent &)> on_records = nullptr;
        acul::unique_function<void(ModelRecordID, ModelFieldID)> on_field_change = nullptr;
        PFN_model_binding_dispatch on_dispatch = nullptr;
        PFN_model_binding_field_dispatch on_field_dispatch = nullptr;
        PFN_model_binding_access_field access_field = nullptr;
    };

    inline Model *find_model(const ModelDB *db, ModelID id)
    {
        if (!db) return nullptr;
        const auto it = db->models.find(id);
        return it != db->models.end() ? const_cast<Model *>(&it->second.model) : nullptr;
    }

    inline const Model *find_const_model(const ModelDB *db, ModelID id)
    {
        if (!db) return nullptr;
        const auto it = db->models.find(id);
        return it != db->models.end() ? &it->second.model : nullptr;
    }

    inline ModelPipelineNode *find_model_pipeline(const ModelDB *db, u64 id)
    {
        if (!db) return nullptr;
        const auto it = db->pipelines.find(id);
        return it != db->pipelines.end() ? it->second.pipeline : nullptr;
    }

    AUIK_EXPORT bool register_model(ModelDB *db, ModelID id, Model model, PFN_destroy_model destroy = nullptr);
    AUIK_EXPORT bool unregister_model(ModelDB *db, ModelID id);
    AUIK_EXPORT bool register_model_pipeline(ModelDB *db, u64 id, ModelPipelineNode *pipeline,
                                             PFN_destroy_model_pipeline destroy = nullptr);
    AUIK_EXPORT void destroy_model_fields(Model *model);
    AUIK_EXPORT ModelID make_generated_model_id();

    template <class T>
    inline Model *make_value_model(ModelDB *db, ModelID model_id = 0u, ModelFieldID field_id = 1u, T value = {})
    {
        if (!db) return nullptr;
        if (model_id == 0u) model_id = make_generated_model_id();
        if (find_model(db, model_id)) return nullptr;
        Model model{};
        model.make_record_id_cb = make_generated_model_record_id;

        ModelRecord record{};
        auto *field = make_model_field<T>(field_id, std::move(value));
        add_model_field(record, field);
        model.add_record(std::move(record));

        if (!register_model(db, model_id, std::move(model), destroy_model_fields))
        {
            field->release();
            return nullptr;
        }
        return find_model(db, model_id);
    }

    AUIK_EXPORT u32 get_model_pipeline_intermediate_stride(ModelPipelineNode *pipeline);
    AUIK_EXPORT void sync_model_db(ModelDB *db);
    AUIK_EXPORT void clear_model_db(ModelDB *db);
    AUIK_EXPORT bool model_pipeline_cache_active(const ModelDB *db);
    AUIK_EXPORT void begin_model_pipeline_cache(ModelDB *db);
    AUIK_EXPORT void end_model_pipeline_cache(ModelDB *db);
    AUIK_EXPORT const ModelPipelineCacheEntry *find_model_pipeline_cache_entry(const ModelDB *db,
                                                                               const ModelPipelineCacheKey &key);
    AUIK_EXPORT const void *find_model_pipeline_cache_value(const ModelDB *db, const ModelPipelineCacheKey &key,
                                                            u32 expected_size = 0u);
    AUIK_EXPORT const void *store_model_pipeline_cache_external(ModelDB *db, const ModelPipelineCacheKey &key,
                                                                const void *data, u32 size);
    AUIK_EXPORT void *alloc_model_pipeline_cache_value(ModelDB *db, const ModelPipelineCacheKey &key, u32 size);
    AUIK_EXPORT void cancel_model_pipeline_cache_value(ModelDB *db, const ModelPipelineCacheKey &key);
    AUIK_EXPORT bool is_model_binding_valid(const ModelBinding &binding);
    AUIK_EXPORT bool is_model_field_access_valid(ModelDB *db, ModelID model_id, const ModelFieldAccess &access);
    AUIK_EXPORT void *get_model_field_access_data(ModelDB *db, ModelID model_id, const ModelFieldAccess &access);
    AUIK_EXPORT const void *get_model_field_access_data(const ModelDB *db, ModelID model_id,
                                                        const ModelFieldAccess &access);
    AUIK_EXPORT bool write_model_field_access(ModelDB *db, ModelID model_id, const ModelFieldAccess &access,
                                              const void *src);
    AUIK_EXPORT ModelField *resolve_model_field(ModelDB *db, ModelID model_id, const ModelFieldAccess &access);
    AUIK_EXPORT const ModelField *resolve_model_field(const ModelDB *db, ModelID model_id,
                                                      const ModelFieldAccess &access);
    AUIK_EXPORT bool process_model_field_access(ModelDB *db, ModelID model_id, const ModelFieldAccess &access,
                                                void *dst);
    AUIK_EXPORT bool process_model_field_access_reverse(ModelDB *db, ModelID model_id, const ModelFieldAccess &access,
                                                        const void *src);

    template <class T>
    inline ModelValueField<T> *get_model_value_field(ModelDB *db, ModelID model_id, const ModelFieldAccess &access)
    {
        auto *field = resolve_model_field(db, model_id, access);
        return field ? static_cast<ModelValueField<T> *>(field) : nullptr;
    }

    template <class T>
    inline const ModelValueField<T> *get_model_value_field(const ModelDB *db, ModelID model_id,
                                                           const ModelFieldAccess &access)
    {
        const auto *field = resolve_model_field(db, model_id, access);
        return field ? static_cast<const ModelValueField<T> *>(field) : nullptr;
    }

    template <class T>
    inline const T *get_model_field_access_value(const ModelDB *db, ModelID model_id, const ModelFieldAccess &access)
    {
        const auto *field = resolve_model_field(db, model_id, access);
        return field ? field->template as<T>() : nullptr;
    }

    template <class T, class U>
    inline bool write_model_field_access_value(ModelDB *db, ModelID model_id, const ModelFieldAccess &access, U &&value)
    {
        if (access.pipeline)
        {
            T next(std::forward<U>(value));
            return process_model_field_access_reverse(db, model_id, access, &next);
        }
        auto *field = get_model_value_field<T>(db, model_id, access);
        if (!field) return false;
        field->value = T(std::forward<U>(value));
        return true;
    }

    template <class T, class U>
    inline bool set_model_field_access_value(ModelDB *db, ModelID model_id, const ModelFieldAccess &access, U &&value)
    {
        if (!write_model_field_access_value<T>(db, model_id, access, std::forward<U>(value))) return false;
        if (auto *field = resolve_model_field(db, model_id, access)) dispatch_model_field(*field);
        return true;
    }

    AUIK_EXPORT ModelRecordID first_model_record_id(ModelDB *db, ModelID model_id);
    AUIK_EXPORT bool default_access_model_binding_field(ModelBinding *binding, ModelRecordID record_id,
                                                        ModelFieldID field_id, ModelFieldAccess *out);
    AUIK_EXPORT bool access_model_binding_field(ModelBinding &binding, ModelRecordID record_id, ModelFieldID field_id,
                                                ModelFieldAccess &out);
    AUIK_EXPORT bool access_model_binding_field(ModelBinding &binding, ModelFieldAccess &out);
    AUIK_EXPORT void dispatch_model_field(ModelField &field);
    AUIK_EXPORT bool process_model_pipeline(ModelDB *db, ModelPipelineNode *pipeline, const void *src, void *dst);
    AUIK_EXPORT bool process_model_pipeline_reverse(ModelDB *db, ModelPipelineNode *pipeline, const void *src,
                                                    void *dst);

    template <class T>
    inline bool read_model_field_access_value(ModelDB *db, ModelID model_id, const ModelFieldAccess &access, T &out)
    {
        if (access.pipeline) return process_model_field_access(db, model_id, access, &out);
        const auto *value = get_model_field_access_value<T>(db, model_id, access);
        if (!value) return false;
        out = *value;
        return true;
    }

    template <class T>
    inline bool read_model_binding_value(ModelBinding &binding, ModelRecordID record_id, ModelFieldID field_id, T &out)
    {
        ModelFieldAccess access{};
        return access_model_binding_field(binding, record_id, field_id, access) &&
               read_model_field_access_value(binding.db, binding.model_id, access, out);
    }

    template <class T>
    inline bool read_model_binding_value(ModelBinding &binding, T &out)
    {
        ModelFieldAccess access{};
        return access_model_binding_field(binding, access) &&
               read_model_field_access_value(binding.db, binding.model_id, access, out);
    }

    template <class T, class U>
    inline bool set_model_binding_value(ModelBinding &binding, ModelRecordID record_id, ModelFieldID field_id,
                                        U &&value)
    {
        ModelFieldAccess access{};
        return access_model_binding_field(binding, record_id, field_id, access) &&
               set_model_field_access_value<T>(binding.db, binding.model_id, access, std::forward<U>(value));
    }

    template <class T, class U>
    inline bool set_model_binding_value(ModelBinding &binding, U &&value)
    {
        ModelFieldAccess access{};
        return access_model_binding_field(binding, access) &&
               set_model_field_access_value<T>(binding.db, binding.model_id, access, std::forward<U>(value));
    }

    inline ModelBinding *make_model_binding(ModelID model_id)
    {
        auto *binding = acul::alloc<ModelBinding>();
        binding->model_id = model_id;
        binding->access_field = default_access_model_binding_field;
        return binding;
    }

    inline ModelBinding *make_model_binding(ModelID model_id, ModelRecordID record_id, ModelFieldID field_id,
                                            ModelPipelineNode *pipeline = nullptr)
    {
        auto *binding = make_model_binding(model_id);
        binding->default_access = {record_id, field_id, pipeline};
        return binding;
    }

    inline ModelBinding *make_value_model_binding(ModelID model_id, ModelFieldID field_id = 1u,
                                                  ModelRecordID record_id = AUIK_MODEL_RECORD_ID_INVALID,
                                                  ModelPipelineNode *pipeline = nullptr)
    {
        return make_model_binding(model_id, record_id, field_id, pipeline);
    }

    AUIK_EXPORT void attach_model_binding(ModelBinding &binding);

    template <class T>
    inline void erase_model_binding_ptr(acul::vector<T *> &items, T *value)
    {
        for (size_t i = 0; i < items.size();)
        {
            if (items[i] == value) items.erase(items.begin() + i);
            else ++i;
        }
    }

    AUIK_EXPORT void detach_model_binding(ModelBinding &binding);

    AUIK_EXPORT ModelRecordID *find_model_binding_record(ModelBinding &binding, ModelRecordID record_id);

    inline void bind_model_record(ModelBinding &binding, ModelRecordID record_id)
    {
        assert(binding.db && binding.db->synced);
        binding.records.push_back(record_id);
    }

    AUIK_EXPORT void rebuild_model_binding_records(ModelBinding &binding);
    AUIK_EXPORT void default_dispatch_model_binding(ModelBinding *binding, const ModelRecordsEvent &event);
    AUIK_EXPORT void default_dispatch_model_binding_field(ModelBinding *binding, ModelRecordID record_id,
                                                          ModelFieldID field_id);

    inline void dispatch_model_binding(ModelBinding &binding, const ModelRecordsEvent &event)
    {
        assert(binding.db && binding.db->synced);
        if (binding.on_dispatch) binding.on_dispatch(&binding, event);
        else default_dispatch_model_binding(&binding, event);
    }

    inline void dispatch_model_binding_field(ModelBinding &binding, ModelRecordID record_id, ModelFieldID field_id)
    {
        assert(binding.db && binding.db->synced);
        if (binding.on_field_dispatch) binding.on_field_dispatch(&binding, record_id, field_id);
        else default_dispatch_model_binding_field(&binding, record_id, field_id);
    }

    inline void Model::dispatch_records(const ModelRecordsEvent &event)
    {
        for (auto *binding : bindings)
            if (binding) dispatch_model_binding(*binding, event);
    }

    AUIK_EXPORT ModelRecord *append_model_record(ModelDB *db, ModelID model_id, ModelRecord record);
    AUIK_EXPORT bool erase_model_record(ModelDB *db, ModelID model_id, ModelRecordID record_id,
                                        bool release_fields = false);
    AUIK_EXPORT bool move_model_record(ModelDB *db, ModelID model_id, ModelRecordID record_id, u32 to_index);
    AUIK_EXPORT void reset_model_records(ModelDB *db, ModelID model_id);
    AUIK_EXPORT bool process_model_pipeline_batch(ModelDB *db, ModelPipelineNode *pipeline, const void *src,
                                                  u32 src_stride, void *dst, u32 dst_stride, u32 count);
    AUIK_EXPORT bool process_model_binding_field_batch(ModelBinding &binding, ModelFieldID field_id,
                                                       ModelPipelineNode *pipeline, void *dst, u32 dst_stride);
} // namespace auik
