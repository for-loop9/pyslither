## Examples

### RL training

A Gymnasium environment that wraps **pyslither** and trains a basic food-eating PPO agent using Stable Baselines 3. Train and test the model:

```bash
python examples/foodnet/train.py --envs 4 --steps 1000000
python examples/foodnet/test_model.py
```

### Interactive viewer

A basic single player demo showing how to use the API. Run it:

```bash
python examples/single_player/main.py
```

---
