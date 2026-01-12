import * as React from "react"
import { Check } from "lucide-react"

import { cn } from "@/lib/utils"

const Checkbox = React.forwardRef<
    HTMLButtonElement,
    { checked?: boolean; onCheckedChange?: (c: boolean) => void; className?: string; id?: string }
>(({ className, checked, onCheckedChange, id, ...props }, ref) => {
    return (
        <button
            type="button"
            role="checkbox"
            aria-checked={checked}
            ref={ref}
            id={id}
            onClick={() => onCheckedChange?.(!checked)}
            className={cn(
                "peer h-4 w-4 shrink-0 rounded-sm border border-primary ring-offset-background focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2 disabled:cursor-not-allowed disabled:opacity-50 flex items-center justify-center",
                checked ? "bg-primary text-primary-foreground" : "bg-transparent",
                className
            )}
            {...props}
        >
            {checked && <Check className="h-3 w-3" />}
        </button>
    )
})
Checkbox.displayName = "Checkbox"

export { Checkbox }
