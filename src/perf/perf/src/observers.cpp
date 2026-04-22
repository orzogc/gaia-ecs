#include "common.h"
#include "registry.h"

void BM_Observer_DiffAncestorCascadeDelete_OnDel(picobench::state& state);
void BM_Observer_DiffAncestorExistingMatches_OnAdd(picobench::state& state);
void BM_Observer_DiffAncestorFilteredObservers_OnAdd(picobench::state& state);
void BM_Observer_DiffAncestorUnrelatedExistingMatches_OnAdd(picobench::state& state);
void BM_Observer_DiffCopyExtDirectFiltered_OnAdd(picobench::state& state);
void BM_Observer_DiffCopyExtExistingMatches_OnAdd(picobench::state& state);
void BM_Observer_DiffCopyExtFiltered_OnAdd(picobench::state& state);
void BM_Observer_DiffPairRelExistingMatches_OnAdd(picobench::state& state);
void BM_Observer_DiffPairRelFiltered_OnAdd(picobench::state& state);
void BM_Observer_DiffRelationEdgeExistingMatches_OnAdd(picobench::state& state);
void BM_Observer_IdenticalTraversed_OnAdd_Local(picobench::state& state);
void BM_Observer_IdenticalTraversed_OnAdd_Shared(picobench::state& state);
void BM_Observer_OnAdd_0_1(picobench::state& state);
void BM_Observer_OnAdd_10_1(picobench::state& state);
void BM_Observer_OnAdd_1_1(picobench::state& state);
void BM_Observer_OnAdd_50_1(picobench::state& state);
void BM_Observer_OnAdd_50_2(picobench::state& state);
void BM_Observer_OnAdd_50_4(picobench::state& state);
void BM_Observer_OnAdd_50_8(picobench::state& state);
void BM_Observer_OnDel_0_1(picobench::state& state);
void BM_Observer_OnDel_10_1(picobench::state& state);
void BM_Observer_OnDel_1_1(picobench::state& state);
void BM_Observer_OnDel_50_1(picobench::state& state);
void BM_Observer_OnDel_50_2(picobench::state& state);
void BM_Observer_OnDel_50_4(picobench::state& state);
void BM_Observer_OnDel_50_8(picobench::state& state);

void register_observers(PerfRunMode mode) {
	switch (mode) {
		case PerfRunMode::Profiling:
			PICOBENCH_SUITE_REG("Profile picks");
			PICOBENCH_REG(BM_Observer_OnAdd_50_4)
					.PICO_SETTINGS_OBS()
					.user_data(NObserverEntities)
					.label("on_add, 50 obs, 4 terms");
			return;
		case PerfRunMode::Sanitizer:
			PICOBENCH_SUITE_REG("Sanitizer picks");
			PICOBENCH_REG(BM_Observer_OnAdd_10_1).PICO_SETTINGS_SANI().user_data(1000).label("on_add, 10 obs");
			return;
		case PerfRunMode::Normal:
			PICOBENCH_SUITE_REG("Observers");
			PICOBENCH_REG(BM_Observer_OnAdd_0_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_add, 0 obs");
			PICOBENCH_REG(BM_Observer_OnAdd_1_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_add, 1 obs");
			PICOBENCH_REG(BM_Observer_OnAdd_10_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_add, 10 obs");
			PICOBENCH_REG(BM_Observer_OnAdd_50_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_add, 50 obs");
			PICOBENCH_REG(BM_Observer_OnAdd_50_2)
					.PICO_SETTINGS_OBS()
					.user_data(NObserverEntities)
					.label("on_add, 50 obs, 2 terms");
			PICOBENCH_REG(BM_Observer_OnAdd_50_4)
					.PICO_SETTINGS_OBS()
					.user_data(NObserverEntities)
					.label("on_add, 50 obs, 4 terms");
			PICOBENCH_REG(BM_Observer_OnAdd_50_8)
					.PICO_SETTINGS_OBS()
					.user_data(NObserverEntities)
					.label("on_add, 50 obs, 8 terms");
			PICOBENCH_REG(BM_Observer_OnDel_0_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_del, 0 obs");
			PICOBENCH_REG(BM_Observer_OnDel_1_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_del, 1 obs");
			PICOBENCH_REG(BM_Observer_OnDel_10_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_del, 10 obs");
			PICOBENCH_REG(BM_Observer_OnDel_50_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_del, 50 obs");
			PICOBENCH_REG(BM_Observer_OnDel_50_2)
					.PICO_SETTINGS_OBS()
					.user_data(NObserverEntities)
					.label("on_del, 50 obs, 2 terms");
			PICOBENCH_REG(BM_Observer_OnDel_50_4)
					.PICO_SETTINGS_OBS()
					.user_data(NObserverEntities)
					.label("on_del, 50 obs, 4 terms");
			PICOBENCH_REG(BM_Observer_OnDel_50_8)
					.PICO_SETTINGS_OBS()
					.user_data(NObserverEntities)
					.label("on_del, 50 obs, 8 terms");
			PICOBENCH_REG(BM_Observer_DiffPairRelFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff pair-rel on_add filtered 1");
			PICOBENCH_REG(BM_Observer_DiffPairRelFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(50)
					.label("observer diff pair-rel on_add filtered 50");
			PICOBENCH_REG(BM_Observer_DiffPairRelFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(200)
					.label("observer diff pair-rel on_add filtered 200");
			PICOBENCH_REG(BM_Observer_DiffPairRelExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff pair-rel local existing 1");
			PICOBENCH_REG(BM_Observer_DiffPairRelExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(100)
					.label("observer diff pair-rel local existing 100");
			PICOBENCH_REG(BM_Observer_DiffPairRelExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1000)
					.label("observer diff pair-rel local existing 1000");
			PICOBENCH_REG(BM_Observer_DiffCopyExtFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff copy_ext on_add filtered 1");
			PICOBENCH_REG(BM_Observer_DiffCopyExtFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(50)
					.label("observer diff copy_ext on_add filtered 50");
			PICOBENCH_REG(BM_Observer_DiffCopyExtFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(200)
					.label("observer diff copy_ext on_add filtered 200");
			PICOBENCH_REG(BM_Observer_DiffCopyExtExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff copy_ext local existing 1");
			PICOBENCH_REG(BM_Observer_DiffCopyExtExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(100)
					.label("observer diff copy_ext local existing 100");
			PICOBENCH_REG(BM_Observer_DiffCopyExtExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1000)
					.label("observer diff copy_ext local existing 1000");
			PICOBENCH_REG(BM_Observer_DiffAncestorExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff ancestor local existing 1");
			PICOBENCH_REG(BM_Observer_DiffAncestorExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(100)
					.label("observer diff ancestor local existing 100");
			PICOBENCH_REG(BM_Observer_DiffAncestorExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1000)
					.label("observer diff ancestor local existing 1000");
			PICOBENCH_REG(BM_Observer_DiffRelationEdgeExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff relation-edge local existing 1");
			PICOBENCH_REG(BM_Observer_DiffRelationEdgeExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(100)
					.label("observer diff relation-edge local existing 100");
			PICOBENCH_REG(BM_Observer_DiffRelationEdgeExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1000)
					.label("observer diff relation-edge local existing 1000");
			PICOBENCH_REG(BM_Observer_DiffAncestorFilteredObservers_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff ancestor filtered observers 1");
			PICOBENCH_REG(BM_Observer_DiffAncestorFilteredObservers_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(50)
					.label("observer diff ancestor filtered observers 50");
			PICOBENCH_REG(BM_Observer_DiffAncestorFilteredObservers_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(200)
					.label("observer diff ancestor filtered observers 200");
			PICOBENCH_REG(BM_Observer_DiffAncestorCascadeDelete_OnDel)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff ancestor delete 1");
			PICOBENCH_REG(BM_Observer_DiffAncestorCascadeDelete_OnDel)
					.PICO_SETTINGS_OBS()
					.user_data(100)
					.label("observer diff ancestor delete 100");
			PICOBENCH_REG(BM_Observer_DiffAncestorCascadeDelete_OnDel)
					.PICO_SETTINGS_OBS()
					.user_data(1000)
					.label("observer diff ancestor delete 1000");
			PICOBENCH_REG(BM_Observer_DiffAncestorUnrelatedExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff ancestor unrelated existing 1");
			PICOBENCH_REG(BM_Observer_DiffAncestorUnrelatedExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(100)
					.label("observer diff ancestor unrelated existing 100");
			PICOBENCH_REG(BM_Observer_DiffAncestorUnrelatedExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1000)
					.label("observer diff ancestor unrelated existing 1000");
			PICOBENCH_REG(BM_Observer_DiffCopyExtDirectFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff copy_ext direct-filtered 1");
			PICOBENCH_REG(BM_Observer_DiffCopyExtDirectFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(50)
					.label("observer diff copy_ext direct-filtered 50");
			PICOBENCH_REG(BM_Observer_DiffCopyExtDirectFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(200)
					.label("observer diff copy_ext direct-filtered 200");
			PICOBENCH_REG(BM_Observer_IdenticalTraversed_OnAdd_Local)
					.PICO_SETTINGS_FOCUS()
					.user_data(128)
					.label("observer identical traversed local 128");
			PICOBENCH_REG(BM_Observer_IdenticalTraversed_OnAdd_Shared)
					.PICO_SETTINGS_FOCUS()
					.user_data(128)
					.label("observer identical traversed shared 128");
			return;
		default:
			return;
	}
}
