import { Select } from '@base-ui/react/select';
import { Field } from '@base-ui/react/field';
import { ChevronDown, Check } from 'lucide-react';
import styles from './BaseSelect.module.css';

interface SelectOption {
    label: string;
    value: string;
}

interface BaseSelectProps {
    label: string;
    options: SelectOption[];
    value: string;
    onChange: (value: string) => void;
    placeholder?: string;
}

export default function BaseSelect({ label, options, value, onChange, placeholder = "Select option" }: BaseSelectProps) {
    return (
        <Field.Root className={styles.Field}>
            <Field.Label className={styles.Label} nativeLabel={false} render={<div />}>
                {label}
            </Field.Label>
            <Select.Root value={value} onValueChange={(v) => onChange(v ?? "")} items={options}>
                <Select.Trigger className={styles.Select}>
                    <Select.Value className={styles.Value} placeholder={placeholder} />
                    <Select.Icon className={styles.SelectIcon}>
                        <ChevronDown size={14} />
                    </Select.Icon>
                </Select.Trigger>
                <Select.Portal>
                    <Select.Positioner className={styles.Positioner} sideOffset={8}>
                        <Select.Popup className={styles.Popup}>
                            <Select.List className={styles.List}>
                                {options.map((opt) => (
                                    <Select.Item key={opt.value} value={opt.value} className={styles.Item}>
                                        <Select.ItemIndicator className={styles.ItemIndicator}>
                                            <Check size={14} className={styles.ItemIndicatorIcon} />
                                        </Select.ItemIndicator>
                                        <Select.ItemText className={styles.ItemText}>{opt.label}</Select.ItemText>
                                    </Select.Item>
                                ))}
                            </Select.List>
                        </Select.Popup>
                    </Select.Positioner>
                </Select.Portal>
            </Select.Root>
        </Field.Root>
    );
}
