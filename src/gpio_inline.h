static inline void gpio_output(int i, int dir)
{
    pinMode(i, !!dir);
}

static inline void gpio_set(int i, int val)
{
    digitalWrite(i, !!val);
}

static inline int gpio_get(int i)
{
    return digitalRead(i);
}
