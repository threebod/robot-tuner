#include <assert.h>
#include <stdio.h>
#include "../../test/mecanum_jog/task_code.h"
#include "../../test/mecanum_jog/mission_actions.h"

int main(void)
{
    static const uint8_t permutations[6][3] = {
        {1, 2, 3}, {1, 3, 2}, {2, 1, 3}, {2, 3, 1}, {3, 1, 2}, {3, 2, 1}};
    static const uint16_t angles[3] = {3065, 2657, 2320};
    static const int16_t horizontal[3] = {55, -240, 435};
    static const char *invalid[] = {
        "", "156+123+516", "156+123+516+231x", "156-123+516+231",
        "056+123+516+231", "157+123+516+231", "156+124+516+231",
        "156+113+516+231", "156+123+516+221", "156+123+516+230",
        "156+123+5a6+231", "156+123+516+ 31"};
    MissionTask task, saved;
    MechanismAction actions[MISSION_PLACE_COUNT], pick[MISSION_PICK_BACK_COUNT];
    MechanismAction secondPick[MISSION_PICK_BACK_COUNT];
    unsigned p, layer, coarse, item, i;
    assert(missionTaskParse("156+123+516+231", &task));
    assert(task.colors[0][0] == 1 && task.colors[0][1] == 5 && task.colors[0][2] == 6);
    assert(task.colors[1][0] == 5 && task.colors[1][1] == 1 && task.colors[1][2] == 6);
    assert(task.rings[0][0] == 1 && task.rings[0][1] == 2 && task.rings[0][2] == 3);
    assert(task.rings[1][0] == 2 && task.rings[1][1] == 3 && task.rings[1][2] == 1);
    saved = task;
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        assert(!missionTaskParse(invalid[i], &task));
        assert(memcmp(&saved, &task, sizeof(task)) == 0);
    }
    assert(!missionTaskParse(NULL, &task));
    assert(!missionTaskParse("156+123+516+231", NULL));
    assert(missionTaskParse("234+312+654+213", &task));
    {
        static const char *codes[] = {
            "123+321+123+312", "452+321+254+312", "156+123+516+231"};
        static const uint8_t expected[3][3] = {{3, 2, 1}, {1, 2, 3}, {2, 1, 3}};
        uint8_t rings[3] = {1, 2, 3};
        unsigned example, a, b, c, pickup, coarseOrder;
        assert(!missionTaskStorageRings(&task, 1U, rings));
        assert(rings[0] == 1 && rings[1] == 2 && rings[2] == 3);
        for (example = 0; example < 3; ++example) {
            assert(missionTaskParse(codes[example], &task));
            saved = task;
            assert(missionTaskStorageRings(&task, 0U, rings));
            assert(memcmp(rings, task.rings[0], sizeof(rings)) == 0);
            assert(missionTaskStorageRings(&task, 1U, rings));
            assert(memcmp(rings, expected[example], sizeof(rings)) == 0);
            assert(memcmp(&saved, &task, sizeof(task)) == 0);
        }
        /* All 20 three-color sets, first-layer positions, second pickup orders,
         * and independent second-batch coarse positions: 4320 combinations. */
        for (a = 1; a <= 4; ++a) for (b = a + 1; b <= 5; ++b)
        for (c = b + 1; c <= 6; ++c) for (p = 0; p < 6; ++p)
        for (pickup = 0; pickup < 6; ++pickup) for (coarseOrder = 0; coarseOrder < 6; ++coarseOrder) {
            task.colors[0][0] = (uint8_t)a;
            task.colors[0][1] = (uint8_t)b;
            task.colors[0][2] = (uint8_t)c;
            for (item = 0; item < 3; ++item) {
                task.rings[0][item] = permutations[p][item];
                task.colors[1][item] = task.colors[0][permutations[pickup][item] - 1];
                task.rings[1][item] = permutations[coarseOrder][item];
            }
            saved = task;
            assert(missionTaskStorageRings(&task, 1U, rings));
            assert(memcmp(&saved, &task, sizeof(task)) == 0);
            missionBuildTaskPlace(actions, rings, 2U, 0U);
            for (item = 0; item < 3; ++item) {
                unsigned platform = item == 0 ? 0 : item == 1 ? 10 : 20;
                unsigned release = item == 0 ? 8 : item == 1 ? 17 : 29;
                unsigned first = permutations[pickup][item] - 1;
                assert(actions[platform].value == item + 1);
                assert(task.colors[1][item] == task.colors[0][first]);
                assert(actions[release].pose.turretDdeg == angles[task.rings[0][first] - 1]);
            }
        }
    }
    for (p = 0; p < 6; ++p) {
        const uint8_t *rings = permutations[p];
        for (layer = 1; layer <= 2; ++layer) for (coarse = 0; coarse <= 1; ++coarse) {
            uint16_t count = coarse ? MISSION_COARSE_PLACE_COUNT : MISSION_PLACE_COUNT;
            missionBuildTaskPlace(actions, rings, (uint8_t)layer, (uint8_t)coarse);
            for (item = 0; item < 3; ++item) {
                unsigned platform = item == 0 ? 0 : item == 1 ? 10 : 20;
                unsigned releasePose = item == 0 ? 8 : item == 1 ? 17 : 29;
                unsigned ring = rings[item] - 1;
                assert(actions[platform].type == MECHANISM_ACTION_PLATFORM);
                assert(actions[platform].value == item + 1);
                assert(actions[releasePose].pose.turretDdeg == angles[ring]);
                assert(actions[releasePose].pose.horizontalDmm == horizontal[ring]);
                assert(actions[releasePose].pose.liftDmm == (layer == 2 ? 650 : 1300));
                assert(actions[releasePose + 1].type == MECHANISM_ACTION_GRIPPER);
                assert(actions[releasePose + 1].value == 1);
            }
            for (i = 0; i < count; ++i)
                if (actions[i].type == MECHANISM_ACTION_POSE)
                    assert(mechanismPoseValid(&actions[i].pose));
            assert(actions[21].pose.horizontalDmm == horizontal[rings[1] - 1]);
        }
        missionBuildTaskPickBack(pick, rings, rings[2], -500);
        missionBuildTaskPickBack(secondPick, rings, rings[2], -450);
        /* Raise at the last release before moving to the first task item. */
        assert(pick[0].type == MECHANISM_ACTION_POSE);
        assert(pick[0].pose.horizontalDmm == horizontal[rings[2] - 1]);
        assert(pick[0].pose.turretDdeg == angles[rings[2] - 1]);
        assert(pick[0].pose.liftDmm == 0);
        for (item = 0; item < 3; ++item) {
            unsigned base = 1 + item * 11;
            unsigned retract = base + 7;
            unsigned ring = rings[item] - 1;
            assert(pick[base].type == MECHANISM_ACTION_PLATFORM);
            assert(pick[base].value == item + 1);
            assert(pick[base + 1].type == MECHANISM_ACTION_GRIPPER && pick[base + 1].value == 1);
            assert(pick[base + 2].pose.liftDmm == 0);
            assert(pick[base + 2].pose.turretDdeg == angles[ring]);
            assert(pick[base + 2].pose.horizontalDmm == horizontal[ring]);
            assert(pick[base + 3].pose.turretDdeg == angles[ring]);
            assert(pick[base + 3].pose.horizontalDmm == horizontal[ring]);
            assert(pick[base + 3].pose.liftDmm == 1300);
            assert(pick[base + 4].type == MECHANISM_ACTION_GRIPPER && pick[base + 4].value == 0);
            assert(pick[base + 5].pose.liftDmm == 0);
            assert(pick[base + 5].pose.turretDdeg == angles[ring]);
            assert(pick[base + 6].pose.horizontalDmm == horizontal[ring]);
            assert(pick[base + 6].pose.turretDdeg == 1320);
            assert(pick[retract].pose.horizontalDmm == -500);
            assert(secondPick[retract].pose.horizontalDmm == -450);
            assert(secondPick[retract + 1].pose.horizontalDmm == -450);
            assert(secondPick[retract + 1].pose.liftDmm == 300);
            assert(secondPick[retract + 2].type == MECHANISM_ACTION_GRIPPER &&
                   secondPick[retract + 2].value == 1);
            assert(secondPick[retract + 3].pose.horizontalDmm == -450);
        }
        assert(pick[MISSION_PICK_BACK_COUNT - 1].type == MECHANISM_ACTION_GRIPPER);
        assert(pick[MISSION_PICK_BACK_COUNT - 2].pose.turretDdeg == 650);
        for (i = 0; i < MISSION_PICK_BACK_COUNT; ++i)
            if (pick[i].type == MECHANISM_ACTION_POSE)
                assert(mechanismPoseValid(&pick[i].pose));
    }
    puts("PASS: task-order placement and recovery for all 6 permutations, raised recovery entry, 4320 same-color stacking combinations, tray offsets");
    return 0;
}
