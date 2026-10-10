#include <auik/model.hpp>
#include <cassert>
#include <cstdio>

void test_model_field_batch()
{
    auik::ModelDB db;
    db.synced = true;
    auik::ModelBinding binding;
    binding.db = &db;

    const auik::ModelFieldChange changes[] = {{11u, 2u}, {12u, 2u}, {12u, 4u}};
    const auik::ModelFieldChangeBatch batch{changes, 3u};

    u32 batch_calls = 0u;
    binding.on_field_change_batch = [&batch_calls](const auik::ModelFieldChangeBatch &event) {
        ++batch_calls;
        assert(event.count == 3u);
        assert(event.changes[0].record_id == 11u && event.changes[0].field_id == 2u);
        assert(event.changes[2].record_id == 12u && event.changes[2].field_id == 4u);
    };
    auik::dispatch_model_binding_field_batch(binding, batch);
    assert(batch_calls == 1u);

    binding.on_field_change_batch = nullptr;
    u32 field_calls = 0u;
    binding.on_field_change = [&field_calls](auik::ModelRecordID, auik::ModelFieldID) { ++field_calls; };
    auik::dispatch_model_binding_field_batch(binding, batch);
    assert(field_calls == 3u);

    binding.on_field_change = nullptr;
    binding.on_field_change_batch = [&batch_calls](const auik::ModelFieldChangeBatch &event) {
        ++batch_calls;
        assert(event.count == 1u);
        assert(event.changes[0].record_id == 21u && event.changes[0].field_id == 5u);
    };
    auik::dispatch_model_binding_field(binding, 21u, 5u);
    assert(batch_calls == 2u);

    binding.db = nullptr;
    puts("PASS: field batches dispatch once and retain single-field compatibility");
}
