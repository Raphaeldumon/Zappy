import pytest

from zappy_train.reward import RewardWeights, shaped_reward


def test_elevation_dominates():
    w = RewardWeights()
    assert shaped_reward(
        leveled_up=True, alive=True, died=False, food_delta=0, won=False, weights=w
    ) == pytest.approx(w.elevation + w.survival)


def test_death_is_negative():
    r = shaped_reward(leveled_up=False, alive=False, died=True, food_delta=0, won=False)
    assert r < 0


def test_win_is_large():
    r = shaped_reward(leveled_up=False, alive=True, died=False, food_delta=0, won=True)
    assert r >= 100.0
